from pathlib import Path
import os

Import("env")

tmp_dir = Path(env.subst("$PROJECT_DIR")) / ".tmp"
tmp_dir.mkdir(exist_ok=True)

for name in ("TEMP", "TMP", "TMPDIR"):
    os.environ[name] = str(tmp_dir)
    env["ENV"][name] = str(tmp_dir)
