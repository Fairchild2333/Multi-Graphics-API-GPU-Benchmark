// Isolated follow-up: identical fixed step counts, then the unmodified engine
// shader, on an offscreen target. Never writes the benchmark's result history.
#define main OriginalMathProbeMain
#include "metal_math_probe.mm"
#undef main

#include <cmath>
#include <vector>

namespace {
struct BurnParams { float time, pass; uint32_t steps, version; };
static_assert(sizeof(BurnParams) == 16, "engine shader ABI");

double DrawFixed(id<MTLCommandQueue> queue, id<MTLRenderPipelineState> pipeline,
                 id<MTLTexture> target, uint32_t steps, float phase, int passes,
                 int firstPass) {
    auto descriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    descriptor.colorAttachments[0].texture = target;
    descriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    descriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    auto commands = [queue commandBuffer];
    commands.label = pipeline.label;
    auto encoder = [commands renderCommandEncoderWithDescriptor:descriptor];
    [encoder setRenderPipelineState:pipeline];
    for (int p = 0; p < passes; ++p) {
        BurnParams params{phase, float(firstPass + p), steps, 3};
        [encoder setFragmentBytes:&params length:sizeof(params) atIndex:0];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    }
    [encoder endEncoding]; [commands commit]; [commands waitUntilCompleted];
    if (commands.status != MTLCommandBufferStatusCompleted) return -1;
    return commands.GPUEndTime - commands.GPUStartTime;
}

template<class Run>
bool Measure(const char* label, uint32_t steps, float phase, int passes, Run run) {
    for (int i = 0; i < 2; ++i) if (run() <= 0) return false;
    std::vector<double> samples;
    for (int i = 0; i < 7; ++i) {
        const double seconds = run();
        if (!(seconds > 0 && std::isfinite(seconds))) return false;
        samples.push_back(seconds);
        std::printf("STAGESAMPLE\tcase=%s\tphase=%.3f\tsteps=%u\tpasses=%d\tsample=%d\tseconds=%.9f\n",
                    label, phase, steps, passes, i, seconds);
    }
    std::sort(samples.begin(), samples.end());
    const double rate = double(kThreads) * steps * passes / samples[3] / 1e9;
    std::printf("MATHSTAGE\tcase=%s\tphase=%.3f\tsteps=%u\tpasses=%d\tmedianSeconds=%.9f\tminSeconds=%.9f\tmaxSeconds=%.9f\tGpixStepsPerSec=%.6f\n",
                label, phase, steps, passes, samples[3], samples.front(), samples.back(), rate);
    return true;
}

bool CheckTexture(id<MTLDevice> device, id<MTLCommandQueue> queue,
                  id<MTLTexture> target, const char* label) {
    auto buffer = [device newBufferWithLength:kThreads * 4 options:MTLResourceStorageModeShared];
    auto commands = [queue commandBuffer]; auto blit = [commands blitCommandEncoder];
    [blit copyFromTexture:target sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
              sourceSize:MTLSizeMake(1280,720,1) toBuffer:buffer destinationOffset:0
      destinationBytesPerRow:1280*4 destinationBytesPerImage:kThreads*4];
    [blit endEncoding]; [commands commit]; [commands waitUntilCompleted];
    if (commands.status != MTLCommandBufferStatusCompleted) return false;
    auto bytes = static_cast<const uint8_t*>(buffer.contents);
    uint64_t hash = 14695981039346656037ull; uint8_t lo = 255, hi = 0;
    for (NSUInteger i=0; i<kThreads*4; ++i) {
        hash = (hash ^ bytes[i]) * 1099511628211ull;
        if (i % 4 != 3) { lo = std::min(lo, bytes[i]); hi = std::max(hi, bytes[i]); }
    }
    std::printf("STAGEOUTPUT\tcase=%s\tfnv1a64=%016llx\tcolorMin=%u\tcolorMax=%u\n",
                label, (unsigned long long)hash, lo, hi);
    return lo < hi;
}
}

int main(int argc, const char* argv[]) {
    @autoreleasepool {
        if (argc < 2) { std::fprintf(stderr, "usage: burn_stage_probe path/to/gpu_burn.metal [fma|discard|capture.gputrace]\n"); return 2; }
        auto device = MTLCreateSystemDefaultDevice();
        if (!device) return 1;
        auto queue = [device newCommandQueue]; NSError* error = nil;
        auto options = [MTLCompileOptions new]; SetMaths(options, true);
        std::printf("GPU: %s\nmathMode=%ld mathFloatingPointFunctions=%ld languageVersion=%lu\n",
                    device.name.UTF8String, (long)options.mathMode,
                    (long)options.mathFloatingPointFunctions, (unsigned long)options.languageVersion);
        auto simple = [device newLibraryWithSource:@(kSource) options:options error:&error];
        NSString* source = [NSString stringWithContentsOfFile:@(argv[1]) encoding:NSUTF8StringEncoding error:&error];
        auto full = source ? [device newLibraryWithSource:source options:options error:&error] : nil;
        if (!simple || !full) { std::fprintf(stderr, "library: %s\n", error.localizedDescription.UTF8String); return 1; }
        auto compute = [device newComputePipelineStateWithFunction:[simple newFunctionWithName:@"k_burn_step"] error:&error];
        auto renderPipeline = [&](id<MTLLibrary> lib, NSString* vs, NSString* fs, NSString* label) {
            auto desc = [MTLRenderPipelineDescriptor new];
            desc.label = label;
            desc.vertexFunction = [lib newFunctionWithName:vs];
            desc.fragmentFunction = [lib newFunctionWithName:fs];
            desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
            return [device newRenderPipelineStateWithDescriptor:desc error:&error];
        };
        auto fragment = renderPipeline(simple, @"v_fullscreen", @"f_burn_step", @"Simplified fragment loop");
        auto engine = renderPipeline(full, @"gpuBurnVertex", @"gpuBurnFragment", @"Unmodified engine shader");
        // Diagnostic control only: all loop/shading code is still read directly
        // from the engine shader. Removing discard changes image semantics;
        // this pipeline must never be used to replace a formal score.
        if ([source componentsSeparatedByString:@"discard_fragment();"].count != 2) return 1;
        NSString* controlSource = [source stringByReplacingOccurrencesOfString:@"discard_fragment();"
                                                                   withString:@"/* diagnostic: no discard */;"];
        auto control = [device newLibraryWithSource:controlSource options:options error:&error];
        auto noDiscard = control ? renderPipeline(control, @"gpuBurnVertex", @"gpuBurnFragment", @"Diagnostic only: no discard") : nil;
        if (!compute || !fragment || !engine || !noDiscard) { std::fprintf(stderr, "pipeline: %s\n", error.localizedDescription.UTF8String); return 1; }
        auto out = [device newBufferWithLength:kThreads*16 options:MTLResourceStorageModePrivate];
        auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:1280 height:720 mipmapped:NO];
        desc.usage = MTLTextureUsageRenderTarget; desc.storageMode = MTLStorageModePrivate;
        auto target = [device newTextureWithDescriptor:desc];
        if (!out || !target) return 1;
        ProbeParams prm{};
        const float a[4]={.999f,.998f,.997f,.996f}, b[4]={.25f,.26f,.27f,.28f};
        std::memcpy(prm.a,a,sizeof(a)); std::memcpy(prm.b,b,sizeof(b));
        if (argc == 2) for (float phase : {.999f, 3.0f}) for (uint32_t steps : {16u,64u,256u}) {
            prm.a[0]=phase; prm.iterations=steps;
            if (!Measure("compute_loop",steps,phase,1,[&]{return RunOnce(queue,compute,out,prm,256);}) ||
                !Measure("fragment_loop",steps,phase,1,[&]{return DrawOnce(queue,fragment,target,prm);}) ||
                !Measure("full_pass0",steps,phase,1,[&]{return DrawFixed(queue,engine,target,steps,phase,1,0);}) ||
                !Measure("full_pass1",steps,phase,1,[&]{return DrawFixed(queue,engine,target,steps,phase,1,1);}) ||
                !Measure("full_two_passes",steps,phase,2,[&]{return DrawFixed(queue,engine,target,steps,phase,2,0);}) ||
                !CheckTexture(device,queue,target,"full_two_passes")) return 1;
        }
        const bool fmaControl = argc > 2 && std::strcmp(argv[2],"fma") == 0;
        if (fmaControl) {
            auto buffered = [device newComputePipelineStateWithFunction:[simple newFunctionWithName:@"k_fma"] error:&error];
            auto literal = [device newComputePipelineStateWithFunction:[simple newFunctionWithName:@"k_fma_literal"] error:&error];
            if (!buffered || !literal) return 1;
            prm.iterations=4096;
            for (int i=0;i<16;++i) if (RunOnce(queue,buffered,out,prm,256)<=0) return 1;
            for (int index : {0,1,1,0}) {
                auto pipeline = index ? literal : buffered;
                std::vector<double> samples;
                for (int i=0;i<9;++i) {
                    double seconds=RunOnce(queue,pipeline,out,prm,256);
                    if (!(seconds>0 && std::isfinite(seconds))) return 1;
                    if (i>=2) samples.push_back(seconds);
                }
                std::sort(samples.begin(),samples.end());
                double tflops=double(kThreads)*16*prm.iterations*2/samples[3]/1e12;
                std::printf("FMAPAIR\tcase=%s\tsteps=%u\tmedianSeconds=%.9f\tminSeconds=%.9f\tmaxSeconds=%.9f\tTFLOPS=%.6f\n",
                            index?"literal":"buffered",prm.iterations,samples[3],samples.front(),samples.back(),tflops);
            }
        }
        const bool discardControl = argc > 2 && std::strcmp(argv[2],"discard") == 0;
        if (discardControl) for (float phase : {.999f,3.0f}) for (uint32_t steps : {16u,64u,256u}) {
            if (!Measure("original_pass0",steps,phase,1,[&]{return DrawFixed(queue,engine,target,steps,phase,1,0);}) ||
                !Measure("original_two_passes",steps,phase,2,[&]{return DrawFixed(queue,engine,target,steps,phase,2,0);}) ||
                !Measure("no_discard_pass0",steps,phase,1,[&]{return DrawFixed(queue,noDiscard,target,steps,phase,1,0);}) ||
                !Measure("no_discard_two_passes",steps,phase,2,[&]{return DrawFixed(queue,noDiscard,target,steps,phase,2,0);}) ||
                !CheckTexture(device,queue,target,"no_discard_two_passes")) return 1;
        }
        if (argc > 2 && !discardControl && !fmaControl) {
            auto manager = [MTLCaptureManager sharedCaptureManager];
            auto capture = [MTLCaptureDescriptor new]; capture.captureObject = queue;
            capture.destination = MTLCaptureDestinationGPUTraceDocument;
            capture.outputURL = [NSURL fileURLWithPath:@(argv[2])];
            if (![manager startCaptureWithDescriptor:capture error:&error]) {
                std::fprintf(stderr,"capture: %s\n",error.localizedDescription.UTF8String); return 1;
            }
            prm.a[0]=.999f; prm.iterations=256;
            bool ok=RunOnce(queue,compute,out,prm,256)>0 && DrawOnce(queue,fragment,target,prm)>0 &&
                    DrawFixed(queue,engine,target,256,.999f,2,0)>0 &&
                    DrawFixed(queue,noDiscard,target,256,.999f,2,0)>0;
            [manager stopCapture];
            if (!ok) return 1;
            std::printf("Capture: %s\n",argv[2]);
        }
        return 0;
    }
}
