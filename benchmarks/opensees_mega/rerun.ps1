$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Push-Location $root
try {
    python .\benchmarks\opensees_mega\harness\compare.py @args
} finally {
    Pop-Location
}
