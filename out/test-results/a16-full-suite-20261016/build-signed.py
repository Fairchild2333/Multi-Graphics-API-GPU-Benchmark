import subprocess,json
from pathlib import Path
p=Path(__file__).resolve().parent
with (p/"signed-build.log").open("w") as f:
 r=subprocess.run(json.loads((p/"signed-build-command.json").read_text()),stdout=f,stderr=subprocess.STDOUT)
(p/"signed-build-exit.json").write_text(json.dumps({"returncode":r.returncode}))
raise SystemExit(r.returncode)
