import { hapTasks } from '@ohos/hvigor-ohos-plugin';
import { HvigorNode } from '@ohos/hvigor';
import { execFileSync } from 'child_process';
import { resolve } from 'path';

export default {
  system: hapTasks,
  plugins: [{
    pluginId: 'mangekyo-shaders',
    apply(node: HvigorNode) {
      // Run after any clean task and before resource discovery. Native
      // compilation generates its shader outputs after resource packaging.
      node.registerTask({
        name: 'stageGpuShaders',
        run() {
          execFileSync(process.execPath, [resolve(node.getNodeDir().getPath(), '../../scripts/stage-ohos-shaders.cjs')], { stdio: 'inherit' });
        },
        postDependencies: ['default@PreBuild']
      });
    }
  }]
}
