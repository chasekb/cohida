# Current GitHub Actions Workflow

The repository's current CI workflow is `.github/workflows/deploy.yml`.
It is the remote build and test gate for cohida when local builds are not run.

## Triggers

- Pushes to `main` and version tags matching `v*.*.*`.
- Pull requests targeting `main`.

## `build-and-test` job

The workflow runs on `ubuntu-latest` and performs these steps:

1. Checks out the repository.
2. Runs `scripts/validate-production-contract.sh` before the image build.
3. Configures Docker Buildx.
4. Logs in to GHCR for non-pull-request events.
5. Builds the `cpp-cohida` Docker context for native `linux/amd64`.
6. Pushes the image for non-pull-request events with branch, semantic-version,
   and commit-SHA metadata tags.
7. Runs the image's unit-test binary while excluding tests that require live
   credentials or a live database connection.

The workflow does not run the obsolete `cpp-build.yml` workflow, install a
separate `jwlawrence/actions-setup-cmake` action, or execute the old
`docker-compose.test.yml` example as a separate CI job. Those instructions
were removed because they did not describe the committed workflow.

## Verification

For a pushed change, verify the Actions run whose `headSha` exactly matches the
pushed commit. A successful older run on the same branch is not sufficient.