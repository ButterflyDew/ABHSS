param(
    [string]$DataDir = $PSScriptRoot,
    [switch]$SkipSha256
)

$ErrorActionPreference = 'Stop'

# Counts printed in Table 1 of the paper.
$paper = [ordered]@{
    Musae       = @{ V = 19109;   E = 400497;    G = 13183 }
    Twitch      = @{ V = 34118;   E = 429113;    G = 3163 }
    Github      = @{ V = 37700;   E = 289003;    G = 4005 }
    Youtube     = @{ V = 1134890; E = 2987624;   G = 8385 }
    DBLP        = @{ V = 2423455; E = 12786329;  G = 127726 }
    Orkut       = @{ V = 3072440; E = 117185083; G = 6288363 }
    LiveJournal = @{ V = 3997962; E = 34681189;  G = 664414 }
    Reddit      = @{ V = 4262834; E = 12502767;  G = 1146657 }
}

# Counts actually present in the author's OneDrive artifact on 2026-07-17.
$artifact = [ordered]@{
    Musae       = @{ V = 19109;   E = 400497;    G = 13183;   Q = 300 }
    Twitch      = @{ V = 34118;   E = 429113;    G = 3163;    Q = 300 }
    Github      = @{ V = 37700;   E = 289003;    G = 4005;    Q = 300 }
    Youtube     = @{ V = 1134890; E = 2987624;   G = 5000;    Q = 300 }
    DBLP        = @{ V = 2497782; E = 12786329;  G = 127726;  Q = 300 }
    Orkut       = @{ V = 3072441; E = 117185083; G = 5000;    Q = 300 }
    LiveJournal = @{ V = 3997962; E = 34681189;  G = 664414;  Q = 2000 }
    Reddit      = @{ V = 4262834; E = 12502767;  G = 1146657; Q = 300 }
}

$suffixes = @('.in', '_beg_pos.bin', '_csr.bin', '_weight.bin', '.g', '3.csv', '5.csv', '7.csv')
$errors = [System.Collections.Generic.List[string]]::new()
$warnings = [System.Collections.Generic.List[string]]::new()

# First verify byte-for-byte download completeness at the metadata level.
$manifestPath = Join-Path $DataDir 'OFFICIAL_ONEDRIVE_MANIFEST.json'
if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.files.Count -ne 64) {
        $errors.Add("The OneDrive manifest contains $($manifest.files.Count) files, expected 64.")
    }
    foreach ($entry in $manifest.files) {
        $path = Join-Path $DataDir $entry.name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            $errors.Add("Missing official file: $path")
        } elseif ((Get-Item -LiteralPath $path).Length -ne [int64]$entry.size) {
            $errors.Add("OneDrive size mismatch for $path; expected $($entry.size) bytes.")
        }
    }
} else {
    $warnings.Add('OFFICIAL_ONEDRIVE_MANIFEST.json is missing; official file lengths were not checked.')
}

if (-not $SkipSha256) {
    $checksumPath = Join-Path $DataDir 'SHA256SUMS.txt'
    if (-not (Test-Path -LiteralPath $checksumPath -PathType Leaf)) {
        $errors.Add('SHA256SUMS.txt is missing; byte-level integrity was not checked.')
    } else {
        $checksums = [ordered]@{}
        foreach ($line in Get-Content -LiteralPath $checksumPath) {
            if ([string]::IsNullOrWhiteSpace($line)) { continue }
            $match = [regex]::Match($line, '^([0-9a-fA-F]{64})\s\s(.+)$')
            if (-not $match.Success) {
                $errors.Add("Malformed SHA-256 line: $line")
                continue
            }
            $name = $match.Groups[2].Value
            if ($checksums.Contains($name)) {
                $errors.Add("Duplicate SHA-256 entry: $name")
                continue
            }
            $checksums[$name] = $match.Groups[1].Value.ToLowerInvariant()
        }
        if ($checksums.Count -ne 64) {
            $errors.Add("SHA256SUMS.txt contains $($checksums.Count) entries, expected 64.")
        }
        foreach ($name in $checksums.Keys) {
            $path = Join-Path $DataDir $name
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
                $errors.Add("Cannot hash missing official file: $path")
                continue
            }
            $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($actual -ne $checksums[$name]) {
                $errors.Add("SHA-256 mismatch for $path")
            }
        }
        Write-Host "Checked SHA-256 for $($checksums.Count) official files."
    }
}

foreach ($dataset in $artifact.Keys) {
    $spec = $artifact[$dataset]
    $paperSpec = $paper[$dataset]
    Write-Host "Checking $dataset ..."

    foreach ($suffix in $suffixes) {
        $path = Join-Path $DataDir ($dataset + $suffix)
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            $errors.Add("Missing: $path")
        }
    }

    $inputPath = Join-Path $DataDir ($dataset + '.in')
    if (Test-Path -LiteralPath $inputPath -PathType Leaf) {
        $header = (Get-Content -LiteralPath $inputPath -TotalCount 1).Trim() -split '\s+'
        if ($header.Count -lt 2 -or [int64]$header[0] -ne $spec.V -or [int64]$header[1] -ne $spec.E) {
            $errors.Add("Unexpected author-artifact header in $inputPath; expected '$($spec.V) $($spec.E)'.")
        }
    }

    $begPath = Join-Path $DataDir ($dataset + '_beg_pos.bin')
    if (Test-Path -LiteralPath $begPath -PathType Leaf) {
        $expectedBytes = [int64]8 * ([int64]$spec.V + 1)
        if ((Get-Item -LiteralPath $begPath).Length -ne $expectedBytes) {
            $errors.Add("Unexpected size for $begPath; expected $expectedBytes bytes.")
        } else {
            $stream = [IO.File]::OpenRead($begPath)
            try {
                [void]$stream.Seek(-8, [IO.SeekOrigin]::End)
                $buffer = [byte[]]::new(8)
                [void]$stream.Read($buffer, 0, 8)
                $lastOffset = [BitConverter]::ToInt64($buffer, 0)
                if ($lastOffset -ne [int64]2 * [int64]$spec.E) {
                    $errors.Add("Unexpected final CSR offset in $begPath; expected $([int64]2 * [int64]$spec.E), got $lastOffset.")
                }
            } finally {
                $stream.Dispose()
            }
        }
    }

    foreach ($binarySuffix in @('_csr.bin', '_weight.bin')) {
        $path = Join-Path $DataDir ($dataset + $binarySuffix)
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            $expectedBytes = [int64]16 * [int64]$spec.E
            $actualBytes = (Get-Item -LiteralPath $path).Length
            if ($actualBytes -ne $expectedBytes) {
                $errors.Add("Truncated/incompatible $path; expected $expectedBytes bytes, got $actualBytes.")
            }
        }
    }

    $groupPath = Join-Path $DataDir ($dataset + '.g')
    if (Test-Path -LiteralPath $groupPath -PathType Leaf) {
        $groupCount = 0
        foreach ($line in [IO.File]::ReadLines((Resolve-Path -LiteralPath $groupPath).Path)) {
            if (-not [string]::IsNullOrWhiteSpace($line)) { $groupCount++ }
        }
        if ($groupCount -ne $spec.G) {
            $errors.Add("Unexpected group count in $groupPath; expected $($spec.G), got $groupCount.")
        }
    }

    foreach ($querySize in @(3, 5, 7)) {
        $path = Join-Path $DataDir ($dataset + $querySize + '.csv')
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            $lineCount = 0
            $badArity = 0
            foreach ($line in [IO.File]::ReadLines((Resolve-Path -LiteralPath $path).Path)) {
                if (-not [string]::IsNullOrWhiteSpace($line)) {
                    $lineCount++
                    if (($line.Trim() -split '\s+').Count -ne $querySize) { $badArity++ }
                }
            }
            if ($lineCount -ne $spec.Q) {
                $errors.Add("Unexpected query count in $path; expected $($spec.Q), got $lineCount.")
            }
            if ($badArity -ne 0) {
                $errors.Add("$path contains $badArity rows whose arity is not $querySize.")
            }
        }
    }

    if ($spec.V -ne $paperSpec.V -or $spec.E -ne $paperSpec.E -or $spec.G -ne $paperSpec.G) {
        $warnings.Add("$dataset differs from Table 1: paper V/E/G=$($paperSpec.V)/$($paperSpec.E)/$($paperSpec.G), artifact=$($spec.V)/$($spec.E)/$($spec.G).")
    }
    if ($spec.Q -ne 300) {
        $warnings.Add("$dataset supplies $($spec.Q) queries per CSV; the paper and experiment scripts use only rows 0..299.")
    }
}

if ($warnings.Count -gt 0) {
    Write-Host ''
    Write-Host 'PAPER/ARTIFACT WARNINGS:' -ForegroundColor Yellow
    $warnings | ForEach-Object { Write-Host "- $_" }
}

if ($errors.Count -gt 0) {
    Write-Host ''
    Write-Host 'DATASET CHECK FAILED:' -ForegroundColor Red
    $errors | ForEach-Object { Write-Host "- $_" }
    exit 1
}

Write-Host ''
Write-Host 'All 64 files passed metadata and structural checks.' -ForegroundColor Green
