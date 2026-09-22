# Smart! CI

GitHub Actions builds every shipping target and runs the automation tests on a **self-hosted
runner** on the maintainer's machine. Hosted runners can't do this job: a build needs the CSS
Unreal engine and a full Satisfactory mod project.

Workflow: [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml)

- **Triggers:** pushes to this repository (any branch; docs-only changes are skipped) and manual
  runs from the Actions tab.
- **Steps:** sync the CI workspace to the commit, build all targets, run all
  `SmartFoundations.*` automation tests, upload logs and the test report as an artifact.
- **Gate:** the build must succeed for every target; the test report must show no failures, no
  tests left unrun, every discovered test passing, and at least 40 tests.

## Scripts

| Script | Purpose |
|---|---|
| `New-CIWorkspace.ps1` | Create or refresh the isolated CI project copy (default `L:\SFCI\SML`). |
| `Invoke-SmartBuild.ps1` | Build all targets in a CI workspace. |
| `Invoke-SmartTests.ps1` | Run the automation tests headless and gate on the report. |
| `SmartCI.psm1` | Shared helpers. |

All scripts refuse to run against a folder without the `.sfci-workspace` marker, so CI can never
build in a working tree. Run them locally the same way the workflow does:

```powershell
.\scripts\ci\Invoke-SmartBuild.ps1 -ProjectRoot L:\SFCI\SML
.\scripts\ci\Invoke-SmartTests.ps1 -ProjectRoot L:\SFCI\SML
```

## Why a separate workspace

Smart! only builds inside a full mod project at `Mods/GameFeatures/SmartFoundations`. Building in
the maintainer's tree would overwrite their binaries and collide with their editor. The CI
workspace mirrors the engine-side project (Source, Content, Config, Plugins, SML; about 14 GB
before build outputs) and holds its own clone of this repository. Refresh it with
`New-CIWorkspace.ps1` after changing SML, the FactoryGame headers, or project plugins.

The first build in a fresh workspace compiles the game module and every project plugin for all
targets, which takes a long time. Later runs are incremental.

## One-time runner setup

Do these yourself; they register a machine with GitHub and install a Windows service.

1. **Create the workspace:** `.\scripts\ci\New-CIWorkspace.ps1`
2. **Choose the runner's account.** A standard (non-admin) local account is safest. It needs:
   - read access to the engine (`C:\Program Files\Unreal Engine - CSS`)
   - full access to `L:\SFCI`
   - `LINUX_MULTIARCH_ROOT` set in its environment (for the Linux server target)
   - Git on its PATH, and PowerShell 7 (`pwsh`)
3. **Install the runner:** GitHub > repository Settings > Actions > Runners > New self-hosted
   runner > Windows x64. Follow the download and `config.cmd` steps shown there, adding:
   - `--labels smart-ci` (the workflow targets `self-hosted, Windows, X64, smart-ci`)
   - `--runasservice` and `--windowslogonaccount` / `--windowslogonpassword` for the account above
   - a short install folder such as `C:\actions-runner`
   **If Windows passwordless sign-in is on** ("only allow Windows Hello sign-in for Microsoft
   accounts"), Windows rejects password logons for services and `config.cmd --runasservice` fails
   with *Invalid windows credentials entered* (the runner still registers). Don't install a
   service: run `run.cmd` in your own session instead, started by a scheduled task at sign-in
   ("run only when user is logged on", normal privileges, no password). The maintainer's machine
   uses this setup; the task is named *Smart CI Runner*.
4. **Lock down the repository** (Settings > Actions > General):
   - Fork pull request workflows: *Require approval for all outside collaborators*
   - Workflow permissions: *Read repository contents*
5. **Optional:** set a repository variable `SF_CI_ROOT` if the workspace is not at `L:\SFCI\SML`.

## Security

The repository is public and the runner executes code on this PC. The workflow therefore has no
`pull_request` or `pull_request_target` trigger: only pushes to this repository and manual runs
can start it, so a fork's pull request can never run on the runner. Keep it that way. To test a
contributor's change, review it, then push it to a branch here yourself.

## Pausing CI

Stop the runner service while you need the machine's full CPU (Services > "GitHub Actions
Runner ..."). Builds also queue behind your own UBT runs automatically (`-WaitMutex`), and the
scripts refuse to start if an editor is already running on the CI workspace.
