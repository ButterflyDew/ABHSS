param(
    [string]$DataDir = $PSScriptRoot
)

$ErrorActionPreference = 'Stop'

$shareUrl = 'https://1drv.ms/f/c/683d9dd9f262486b/Ek6Fl_brQzhDnI2cmhGIHxMBQ-L1ApeSqxwZKE4NBsDXSQ?e=3RBc8S'
$datasets = @('Twitch', 'Musae', 'Github', 'Youtube', 'Orkut', 'DBLP', 'Reddit', 'LiveJournal')
$suffixes = @('.in', '_beg_pos.bin', '_csr.bin', '_weight.bin', '.g', '3.csv', '5.csv', '7.csv')

New-Item -ItemType Directory -Force -Path $DataDir | Out-Null

$missing = [System.Collections.Generic.List[string]]::new()
foreach ($dataset in $datasets) {
    foreach ($suffix in $suffixes) {
        $name = $dataset + $suffix
        if (-not (Test-Path -LiteralPath (Join-Path $DataDir $name) -PathType Leaf)) {
            $missing.Add($name)
        }
    }
}

if ($missing.Count -eq 0) {
    Write-Host 'All 64 author files are already present.' -ForegroundColor Green
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'verify_data.ps1') -DataDir $DataDir
    exit $LASTEXITCODE
}

Write-Host "Missing $($missing.Count) author files:" -ForegroundColor Yellow
$missing | ForEach-Object { Write-Host "- $_" }
Write-Host ''
Write-Host 'Official public folder:'
Write-Host $shareUrl
Write-Host ''
Write-Host 'The share has been migrated to Microsoft personal SharePoint. The old ?download=1 folder URL now returns an HTML shell instead of a ZIP, so this script deliberately does not save that page as a fake archive.'
Write-Host 'Open the official folder in a browser, download the missing files, place them in this directory, and rerun verify_data.ps1. Use OFFICIAL_ONEDRIVE_MANIFEST.json to check the exact official byte lengths.'
exit 2
