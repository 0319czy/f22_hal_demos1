#!/usr/bin/env bash
# Set up a local dev environment: mise-managed Python and uv (both in
# .mise.toml — Python pinned to 3.12, uv tracking latest), a uv-created
# .venv, and the project's dependencies. Offers to install mise itself if
# it's missing, and to run the smoke tests once the environment is ready.
#
# .mise.toml declares `virtualenv = ".venv"` on the python tool, so once
# .venv exists on the right version, mise's own shell hook (`mise activate`)
# activates it automatically on cd — no `uv run` / `source activate` needed
# day to day. That declaration is also why `mise exec -- python` can't be
# used below to find the *target* interpreter: once .venv exists, mise
# redirects `python` to .venv/bin/python, which would make the version
# check below compare .venv against itself and never catch drift. `mise
# where python` isn't affected by the virtualenv redirect, so it's used
# instead.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

if ! command -v mise >/dev/null 2>&1; then
  echo "'mise' (https://mise.jdx.dev/) is required but not on PATH."
  reply=""
  read -r -p "Install it now via 'curl https://mise.run | sh'? [y/N] " reply || true
  case "$reply" in
    [Yy]*)
      curl -fsSL https://mise.run | sh
      export PATH="$HOME/.local/bin:$PATH"
      command -v mise >/dev/null 2>&1 || {
        echo "error: mise installed but still not on PATH — open a new shell and re-run ./init.sh" >&2
        exit 1
      }
      ;;
    *)
      echo "error: install mise and re-run ./init.sh" >&2
      exit 1
      ;;
  esac
fi

mise trust .
mise install
uv() { mise exec -- uv "$@"; }
python_bin="$(mise where python)/bin/python3"
target_version="$("$python_bin" -c 'import sys; print("%d.%d" % sys.version_info[:2])')"

if [ -d .venv ]; then
  venv_version="$(sed -n 's/^version_info = \([0-9]*\.[0-9]*\).*/\1/p' .venv/pyvenv.cfg 2>/dev/null || true)"
  if [ "$venv_version" = "$target_version" ]; then
    echo ".venv already on Python ${target_version}, skipping creation"
  else
    echo ".venv is Python ${venv_version:-unknown}, pinned version is ${target_version}; recreating"
    uv venv --python "$python_bin" --clear .venv
  fi
else
  uv venv --python "$python_bin" .venv
fi

uv pip install --python .venv -r requirements.txt

echo "Done. If mise is hooked into your shell (mise activate), .venv activates"
echo "automatically next time you're in this directory — just run 'python propeller.py'."
echo "Otherwise: 'uv run python propeller.py', or 'source .venv/bin/activate' once."

reply=""
read -r -p "Run smoke tests now? Builds every preset + a coverage matrix (~12 builds, a minute or more) [y/N] " reply || true
case "$reply" in
  [Yy]*) uv run python test_presets.py ;;
  *) echo "Skipped. Run 'uv run python test_presets.py' anytime to verify the environment." ;;
esac
