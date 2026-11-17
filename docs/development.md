# Development workflow

Repository: [MasCorra/esp32-siemens-aws-gateway](https://github.com/MasCorra/esp32-siemens-aws-gateway). The default branch is `master`.

## Clone and set up

```powershell
git clone https://github.com/MasCorra/esp32-siemens-aws-gateway.git
Set-Location esp32-siemens-aws-gateway
```

Follow the [build guide](build.md) to prepare dependencies and run checks. Keep local credentials in `secrets.h`; commit only the example template.

## Review and commit changes

From the repository root:

```powershell
git status --short
./scripts/check-repository.ps1
git diff
git add -- <changed-files>
git diff --cached --stat
git diff --cached --check
git diff --cached
git commit -m "Describe the change"
git push
```

Replace `<changed-files>` with the intended paths. Review the staged diff before committing, including newly added files. Historical sketches retain their original formatting; whitespace warnings in those files do not by themselves indicate firmware errors.

GitHub Desktop provides the same workflow: review Changes, enter a commit summary, commit to the current branch, then select **Push origin**. CI checks each push on Windows and Linux.

## Credentials and build output

`.gitignore` excludes `secrets.h`, separate certificates and keys, build output and local analysis. Do not force-add ignored credentials or paste real values into tracked sketches.

If credentials were published, removing them from Git does not revoke them. Rotate or revoke exposed credentials before cleaning history; see [GitHub's guidance](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/removing-sensitive-data-from-a-repository).

## Commissioning

Passing software tests does not validate a physical installation. Before deployment, verify PLC reads, MQTT payloads, reconnection behavior, GPIO commands and AWS permissions on the target bench. See [firmware settings and limitations](firmware.md).
