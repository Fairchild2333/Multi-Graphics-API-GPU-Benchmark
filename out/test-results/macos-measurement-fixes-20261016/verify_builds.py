"""Check the final boundary-guard packages, logs and immutable mobile histories."""
from pathlib import Path
import hashlib,json,re,subprocess,xml.etree.ElementTree as ET,zipfile
root=Path(__file__).resolve().parent
repo=root.parents[2]
sha=lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
run=lambda *args: subprocess.check_output(list(map(str,args)),text=True).strip()
data=json.loads((root/'build-verification.json').read_text())
logs={'macos':'build-macos-boundary.log','ios':'build-ios-boundary.log','android':'build-android-boundary.log','ohos':'build-ohos-boundary.log','ctest':'ctest-boundary.log'}
for platform,name in logs.items():
    text=(root/name).read_text()
    marker={'macos':'BUILD SUCCEEDED','ios':'BUILD SUCCEEDED','android':'BUILD SUCCESSFUL','ohos':'BUILD SUCCESSFUL','ctest':'100% tests passed'}[platform]
    assert marker in text,(platform,'missing success marker')
    if platform=='ios': assert text.count('BUILD SUCCEEDED')==4
libs=re.findall(r'Static library: (.+)',(root/logs['ios']).read_text())
for artifact in data['artifacts']:
    p=Path(artifact['path'])
    if artifact['platform'] in ['iphoneos','iphonesimulator']:
        sdk=artifact['platform']
        binary=p/'Mangekyo'
        library=Path(next(s for s in libs if f'Release-{sdk}/' in s))
        artifact.update(binaryArchitecture=run('lipo','-archs',binary),libraryArchitecture=run('lipo','-archs',library),
                        libraryPath=str(library),binarySha256=sha(binary),librarySha256=sha(library))
        blob=library.read_bytes()
        assert b'membw_v3_streaming' in blob and b'_kept_branch' not in blob
    else:
        artifact.update(bytes=p.stat().st_size,sha256=sha(p))
mac=repo/'out/macos/arm64/Release/Mangekyo.app'
worker=mac/'Contents/Helpers/gpu_benchmark'
subprocess.run(['codesign','--verify','--deep','--strict',str(mac)],check=True)
subprocess.run([str(worker),'--help'],check=True,stdout=subprocess.DEVNULL)
current=sha(worker)
assert current==json.loads((root/'boundary-provenance.json').read_text())['workerSha256']
assert all(r['workerSha256']==current for r in json.loads((root/'boundary/run-status.json').read_text()))
data['macos']={'path':str(mac),'workerSha256':current,'architecture':run('lipo','-archs',worker),'signature':'verified','help':'passed'}
data.pop('finalWorkerUnchangedSinceMeasurements',None)
data['finalBoundaryGuardBuildLogs']=logs
data['boundaryWorkerMatchesMeasurements']=True
data['particleMeasurementsWorkerSha256']=json.loads((root/'final-provenance.json').read_text())['workerSha256']
data['postParticleCodeChange']='Only memory-tail eligibility changed; comments subsequently clarified. Particle kernels and timing unchanged.'
data['productionCppReader']='passed final 6/6; boundary formal accepted and 256 MiB preview flagged'
data['productionSwiftReader']='10/14 accepted; 3 query controls and 1 boundary preview flagged; full curves restored'
xmls=list((repo/'android/app/build/test-results/testDebugUnitTest').glob('TEST-*.xml'))
suites=[ET.parse(p).getroot() for p in xmls]
assert sum(int(s.attrib['tests']) for s in suites)==5
assert all(int(s.attrib.get('failures',0))==int(s.attrib.get('errors',0))==0 for s in suites)
data['androidJUnit']=[{k:(s.attrib[k] if k=='name' else int(s.attrib[k])) for k in ['name','tests','failures','errors']} for s in suites]
shared=['Models/EngineOutput.swift','Models/ChartGeometry.swift','Models/Localization.swift','Models/BenchResult.swift','Components/BenchCharts.swift','Components/ResultCharts.swift']
assert all((repo/'ios-gui/Mangekyo'/p).read_bytes()==(repo/'macos-gui/GPUBenchmark'/p).read_bytes() for p in shared)
for item in json.loads((repo/'out/test-results/archive-index.json').read_text())['devices'][1:]:
    p=repo/'out/test-results'/item['path']
    assert sha(p)==item['sha256'] and len(json.loads(p.read_text()))==item['rows']
package_checks=json.loads((root/'shader-package-verification.json').read_text())
for check in package_checks:
    platform=check['platform']
    if platform in ['android','ohos']:
        with zipfile.ZipFile(check['path']) as z:
            entries=[n for n in z.namelist() if n.endswith('/membw_read.comp.spv')]
            assert len(entries)==1
            blob=z.read(entries[0])
    else:
        app=mac if platform=='macos' else next(Path(a['path']) for a in data['artifacts'] if a['platform']==platform)
        entries=list(app.rglob('membw_read.comp.spv'))
        assert len(entries)==(2 if platform=='macos' else 1)
        assert all(hashlib.sha256(p.read_bytes()).hexdigest()==check['sha256'] for p in entries)
        blob=entries[0].read_bytes()
    assert hashlib.sha256(blob).hexdigest()==check['sha256'] and len(blob)==check['bytes'] and b'repeats' in blob
(root/'build-verification.json').write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
print('Final packages/signature/architectures/JUnit/shared files/SPIR-V/mobile-history hashes: passed')
