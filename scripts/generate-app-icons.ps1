#requires -Version 7.0
# Authoring entry point; normal builds consume committed assets.
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
& python (Join-Path $projectRoot 'assets/brand/current/source/build_identity.py') --runtime
if($LASTEXITCODE -ne 0){throw 'Brand asset generation failed; see assets/brand/current/README.md for authoring dependencies.'}
