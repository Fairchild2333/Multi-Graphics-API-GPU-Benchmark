// Run before Hvigor reads resources: native compilation runs too late to
// include newly generated shaders in the first HAP of a clean checkout.
const fs = require('fs');
const path = require('path');
const { spawnSync, execFileSync } = require('child_process');
const repo = path.resolve(__dirname, '..');
const source = path.join(repo, 'shaders');
const output = path.join(repo, 'ohos/entry/src/main/resources/rawfile/shaders');
const base = ['particle.vert', 'particle.frag', 'compute.comp', 'fractal.vert', 'gpu_burn.frag'];
const names = [...base, ...fs.readdirSync(source).filter(n => /^(cinematic_liquid_.*\.(comp|vert|frag)|mls_mpm_.*\.comp|membw_.*\.comp)$/.test(n))];
const candidates = [process.env.GLSLC];
if (process.env.VULKAN_SDK) for (const dir of ['bin', 'Bin']) candidates.push(path.join(process.env.VULKAN_SDK, dir, process.platform === 'win32' ? 'glslc.exe' : 'glslc'));
candidates.push('glslc');
if (process.platform === 'darwin') candidates.push('/opt/homebrew/bin/glslc', '/usr/local/bin/glslc');
const compiler = candidates.filter(Boolean).find(p => spawnSync(p, ['--version'], {stdio:'ignore'}).status === 0);
if (!compiler) throw new Error('glslc not found. Install the Vulkan SDK or set GLSLC to its compiler path.');
fs.mkdirSync(output, {recursive:true});
for (const name of names) {
  const input = path.join(source, name), target = path.join(output, name + '.spv');
  if (fs.existsSync(target) && fs.statSync(target).mtimeMs >= fs.statSync(input).mtimeMs) continue;
  const temporary = target + '.' + process.pid + '.tmp';
  try {
    execFileSync(compiler, ['--target-env=vulkan1.0', input, '-o', temporary], {stdio:'inherit'});
    fs.renameSync(temporary, target);
  } finally { if (fs.existsSync(temporary)) fs.unlinkSync(temporary); }
}
for (const name of ['compute_gl.comp', 'particle_gl.vert', 'particle_gl.frag', 'fractal_gl.vert', 'gpu_burn_gl.frag', 'gpu_burn_es2.vert', 'gpu_burn_es2.frag']) {
  const target = path.join(output, name), content = fs.readFileSync(path.join(source, name));
  if (!fs.existsSync(target) || !content.equals(fs.readFileSync(target))) fs.writeFileSync(target, content);
}
console.log(`HarmonyOS shaders ready: ${names.length} SPIR-V + 7 GLES sources`);
