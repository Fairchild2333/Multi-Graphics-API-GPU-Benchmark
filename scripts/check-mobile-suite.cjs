// Exercise the production OHOS models/state machine without a device. This is
// not an ArkUI/HAP compiler; pass the path to an installed TypeScript library.
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert/strict');
const ts = require(process.argv[2] || 'typescript');
const root = path.resolve(__dirname, '../ohos/entry/src/main/ets/common');
let now = 100000, busy = false, nativeError = '', starts = 0, probes = 0;
let caps = {vulkan:true, liquid:true, vulkanMaxParticles:16777216, gles31:true, glesMaxParticles:16777216, gles2:true};
const files = new Map(), modules = new Map();
const fakeFs = {
  OpenMode: {CREATE:1, READ_WRITE:2, APPEND:4, TRUNC:8},
  accessSync: p => files.has(p), mkdirSync: p => files.set(p, ''),
  readTextSync: p => files.get(p) || '[]',
  openSync: (p, flags) => { if (!files.has(p) || (flags & 8)) files.set(p, ''); return {fd:p}; },
  writeSync: (fd, text) => files.set(fd, files.get(fd) + text), closeSync: () => {},
};
let stopWorks = true;
let nativeLines = [];
const bridge = {
  suiteCapabilities: () => { probes++; return JSON.stringify(caps); },
  pollOutput: () => nativeLines.splice(0), isRunning: () => busy, isCliRunning: () => busy,
  hasSurface: () => true, lastError: () => nativeError, cliExitCode: () => 0,
  startWorkload: () => { starts++; busy = true; return true; },
  startCli: args => { assert(!args.includes('--cpu-rounds')); starts++; busy = true; return true; },
  stopWorkload: () => { if (stopWorks) busy = false; },
};
function load(name) {
  if (name === './I18n') return {tr:(en, zh) => en};
  if (name === 'libmangekyo.so') return {__esModule:true,default:bridge};
  if (name === '@kit.CoreFileKit') return {fileIo:fakeFs};
  if (name === '@kit.PerformanceAnalysisKit') return {hilog:{info(){}}};
  name = name.replace('./','');
  if (modules.has(name)) return modules.get(name).exports;
  const module = {exports:{}}; modules.set(name,module);
  const source = fs.readFileSync(path.join(root, name + '.ets'), 'utf8');
  const result = ts.transpileModule(source, {reportDiagnostics:true, compilerOptions:{
    target:ts.ScriptTarget.ES2022, module:ts.ModuleKind.CommonJS, experimentalDecorators:true,
  }});
  assert(!result.diagnostics.some(d => d.category === ts.DiagnosticCategory.Error), name);
  const context = {module, exports:module.exports, require:load, console,
    Date:{now:()=>now}, isFinite, Math, JSON, Array,
    ObservedV2:c=>c, Trace:()=>{}};
  vm.runInNewContext(result.outputText, context, {filename:name});
  return module.exports;
}
const production = load('MobileSuite');
const results = load('ResultsStore');
const {cpuRun} = load('EngineOutput');
const plan = production.supportedSuiteJobs;
assert.equal(plan(caps,'all').length,29);
assert.equal(plan(caps,'gpu').length,27);
assert.equal(plan({...caps,vulkan:false,gles31:false,gles2:false},'cpu').length,2);
assert.equal(plan({...caps,liquid:false,glesMaxParticles:4194304},'all').length,26);
assert.equal(new Set(plan(caps,'all').map(j=>j.id)).size,29);

function record(overrides={}) {
  const r = new results.ResultEntry();
  Object.assign(r,{id:'new',workload:'stream',graphicsApi:'Vulkan',particleCount:4194304,headless:true,
    score:20,durationSec:15,avgComputeMs:10,avgTotalGpuMs:10,avgFrameTimeMs:10,stableScore:20},overrides);
  return r;
}
const stream = new production.SuiteJob('stream','vulkan','stream',4194304,0,true);
assert(record().matches(stream));
assert(!record({durationSec:3}).matches(stream));
assert(!record({graphicsApi:'OpenGL ES'}).matches(stream));
assert(!record({particleCount:1048576}).matches(stream));
const memoryJob = {api:'vulkan', workload:'membw', particles:0, steps:0, headless:true};
const memoryRecord = {workload:'membw',workloadVersion:'membw_v2_verified_ohos_preview',
  workloadConfig:'dataVerified=1;writeCopyChecks=2;timestampAnomalies=0'};
assert(record(memoryRecord).matches(memoryJob));
for (const config of ['dataVerified=0;writeCopyChecks=2;timestampAnomalies=0',
  'dataVerified=1;writeCopyChecks=0;timestampAnomalies=0',
  'dataVerified=1;writeCopyChecks=2;timestampAnomalies=1'])
  assert(!record({...memoryRecord,workloadConfig:config}).matches(memoryJob));
assert(!record({...memoryRecord,workloadVersion:'membw_v1_ohos_preview'}).matches(memoryJob));
assert.equal(record().reviewReasons().length,0);
for (const overrides of [{avgComputeMs:0.002},{avgTotalGpuMs:20},{avgComputeMs:8,avgRenderMs:8},{stableScore:0},{throughputDropPct:8}])
  assert(record(overrides).reviewReasons().length > 0);
assert.equal(record({workload:'cinematic_liquid',headless:false,stableScore:0,throughputDropPct:8}).reviewReasons().length,0);
assert.equal(record({workload:'cpu_membw',avgTotalGpuMs:20,stableScore:0}).reviewReasons().length,0);

const screen = [];
let runner = new production.MobileSuiteRunner();
runner.start('/data','cpu',(show,job)=>screen.push(show),()=>{});
assert.equal(probes,0,'CPU-only must not probe a GPU');
runner.tick(); assert(busy);
nativeLines.push('CPU_PROGRESS\tmode=multi\tphase=measure\toverall_fraction=0.4',
  'CPU_RESULT\tkind=summary\tscore=10\tunit=MWork/s',
  'CPU_RESULT\tkind=multi\tscore=40\tunit=MWork/s\tthread_count=4');
runner.tick();
assert.equal(cpuRun.progress,0.4,'The suite must publish real parsed CPU progress');
assert(cpuRun.singleSummary.includes('10') && cpuRun.multiSummary.includes('40'));
runner.cancel(); runner.tick();
assert.equal(production.suiteRun.running,false); assert(production.suiteRun.cancelled);
assert.equal(starts,1,'Cancel cannot launch the second CPU phase');
const computeSummary = cpuRun.singleSummary;
cpuRun.begin(true,true);
cpuRun.consumeAll(['CPUMEM_META\tall_threads=4','CPUMEM_POINT\tmode=single\tkernel=read\tbytes=4096\tgib_s=20']);
assert.equal(cpuRun.singleSummary,computeSummary,'Memory phase must retain compute summaries');
assert.equal(cpuRun.allThreads,4);
cpuRun.finish(130);

caps = {...caps,vulkan:false,gles31:false,gles2:true};
runner = new production.MobileSuiteRunner();
runner.start('/data','gpu',(show)=>screen.push(show),()=>{});
runner.tick(); now += 601; runner.tick(); assert.equal(starts,2);
const es2 = record({id:'es2-16',workload:'gpu_burn',graphicsApi:'OpenGL ES 2.0',headless:false,particleCount:0,
  workloadVersion:'gpu_burn_es2_legacy_v1_fixed_steps_16_ohos_preview_sync_timing',workloadConfig:'steps=16'});
files.set('/data/results/results.json',JSON.stringify([es2])); busy=false; runner.tick();
assert.equal(production.suiteRun.finished,1); assert.equal(production.suiteRun.failures,0);
runner.cancel(); runner.tick();
assert.equal(starts,2,'Cancel during cooldown cannot start another GPU job');
assert.equal(JSON.parse(files.get('/data/results/results.json')).length,1,'Old raw results remain intact');

now += 100000;
runner = new production.MobileSuiteRunner(); runner.start('/data','gpu',()=>{},()=>{});
runner.tick(); now += 601; runner.tick(); busy=false; runner.tick();
assert.equal(production.suiteRun.failures,1,'An existing result cannot satisfy a new job');
runner.cancel(); runner.tick();

now += 100000; stopWorks=false;
runner = new production.MobileSuiteRunner(); runner.start('/data','gpu',()=>{},()=>{}); runner.tick(); now += 601; runner.tick();
const beforeTimeout = starts;
now += 180001; runner.tick(); now += 30001; runner.tick();
assert.equal(production.suiteRun.running,false);
assert.equal(starts,beforeTimeout,'An unresponsive worker must abort the remaining queue');
console.log('OHOS production models/state machine: scope, limits, result matching, review, cancellation, stale history and timeout checks passed');
