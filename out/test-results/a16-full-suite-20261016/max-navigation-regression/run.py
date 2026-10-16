from pathlib import Path
import subprocess,json
p=Path(__file__).resolve().parent
with (p/"test.log").open("w") as f:
 r=subprocess.run(json.loads((p/"test-command.json").read_text()),stdout=f,stderr=subprocess.STDOUT)
(p/"exit.json").write_text(json.dumps({"returncode":r.returncode}))
raise SystemExit(r.returncode)
