import json,subprocess
from pathlib import Path
p=Path(__file__).resolve().parent
with (p/"ui-test.log").open("w") as f:
 r=subprocess.run(json.loads((p/"test-command.json").read_text()),stdout=f,stderr=subprocess.STDOUT)
(p/"ui-test-exit.json").write_text(json.dumps({"returncode":r.returncode}))
raise SystemExit(r.returncode)
