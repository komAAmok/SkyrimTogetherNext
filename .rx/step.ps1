$ErrorActionPreference='Continue'
Set-Location .rx
foreach ($needed in 'SkyrimTogetherSKSE.dll','SkyrimTogetherRuntime.dll','SkyrimTogetherRuntime_1_5.dll') {
  $path = Join-Path $env:STR_BUILD_DIR $needed
  if (!(Test-Path $path)) { throw "build output missing before packaging: $needed" }
}

$args = @{
  Stage = 'mod-pack-dryrun'
  Dist = 'dist-dryrun'
  BuildDir = $env:STR_BUILD_DIR
  Version = 'v0.0.0-dryrun'
}
if (Test-Path 'plugin-artifacts') { $args['ArtifactsRoot'] = 'plugin-artifacts' }
& ../Tools/Packaging/New-STNModPackage.ps1 @args
if ($LASTEXITCODE -ne 0) { throw "packaging dry run failed (exit $LASTEXITCODE)" }

$zip = Get-ChildItem 'dist-dryrun' -Filter '*.zip' | Select-Object -First 1
if (-not $zip) { throw 'packaging produced no archive' }
Write-Host "dry run produced $($zip.Name)"
