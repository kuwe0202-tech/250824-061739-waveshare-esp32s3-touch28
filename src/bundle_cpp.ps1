
<#  bundle_cpp.ps1 v1.4.1
    - Entfernt C/C++/INO-Kommentare
    - Bündelt .cpp/.c/.h/.ino bis MaxChars "Code" pro Bundle
    - Große Dateien:
        * Restplatz des aktuellen Bundles mit Part 1 füllen
        * mittlere Parts eigenes Bundle
        * letzter Part bleibt offen für Folge-Dateien
    - Verifikation robust gegen LF/CRLF
    - Am Ende: Gesamtsumme aller Bundles
#>

param(
  [string]$Root = ".",
  [int]$MaxChars = 100000,
  [string]$OutDir = ".\bundle_out",
  [string[]]$Extensions = @(".cpp",".c",".h",".ino"),
  [string[]]$ExcludeSubpaths = @("\.git\", "\build\", "\dist\", "\node_modules\", "\.pio\", "\.vs\", "\.vscode\")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Pfade und Gesamtsumme
$Root  = (Resolve-Path -LiteralPath $Root).Path
if (-not [IO.Path]::IsPathRooted($OutDir)) { $OutDir = Join-Path $Root $OutDir }
$script:grandTotal = 0

function Get-RelativePath([string]$basePath, [string]$fullPath) {
  $base = (Resolve-Path $basePath).Path
  $full = (Resolve-Path $fullPath).Path
  if ($full.StartsWith($base)) { return $full.Substring($base.Length).TrimStart('\','/') }
  return $full
}
function Path-IsExcluded([string]$path, [string[]]$patterns) { foreach ($p in $patterns) { if ($path -like "*$p*") { return $true } } return $false }

function Strip-CStyleComments([string]$text) {
  $len=$text.Length; $sb=New-Object System.Text.StringBuilder; $i=0
  $STATE_CODE=0;$STATE_SL=1;$STATE_ML=2;$STATE_STR=3;$STATE_CHR=4; $state=$STATE_CODE
  while ($i -lt $len) {
    $ch=$text[$i]; $nch= if ($i+1 -lt $len){$text[$i+1]} else {[char]0}
    switch ($state) {
      $STATE_CODE {
        if ($ch -eq '/' -and $nch -eq '/') { $state=$STATE_SL; $i+=2; continue }
        if ($ch -eq '/' -and $nch -eq '*') { $state=$STATE_ML; $i+=2; continue }
        if ($ch -eq '"') { [void]$sb.Append($ch); $state=$STATE_STR; $i++; continue }
        if ($ch -eq "'") { [void]$sb.Append($ch); $state=$STATE_CHR; $i++; continue }
        [void]$sb.Append($ch); $i++; continue
      }
      $STATE_SL { if ($ch -eq "`r" -or $ch -eq "`n") { $state=$STATE_CODE; [void]$sb.Append($ch); $i++; continue } $i++; continue }
      $STATE_ML { if ($ch -eq '*' -and $nch -eq '/') { $state=$STATE_CODE; $i+=2; continue } $i++; continue }
      $STATE_STR { if ($ch -eq '\') { [void]$sb.Append($ch); if ($i+1 -lt $len){[void]$sb.Append($text[$i+1]);$i+=2;continue} $i++;continue }
                   if ($ch -eq '"'){[void]$sb.Append($ch);$state=$STATE_CODE;$i++;continue}
                   [void]$sb.Append($ch);$i++;continue }
      $STATE_CHR { if ($ch -eq '\') { [void]$sb.Append($ch); if ($i+1 -lt $len){[void]$sb.Append($text[$i+1]);$i+=2;continue} $i++;continue }
                   if ($ch -eq "'"){[void]$sb.Append($ch);$state=$STATE_CODE;$i++;continue}
                   [void]$sb.Append($ch);$i++;continue }
    }
  }
  return $sb.ToString()
}

function Split-IntoChunks([string]$text, [int]$max) {
  $chunks=New-Object 'System.Collections.Generic.List[string]'; $start=0
  while ($start -lt $text.Length) {
    $len=[Math]::Min($max,$text.Length-$start); $end=$start+$len
    if ($end -lt $text.Length) {
      $slice=$text.Substring($start,$len); $lastNl=$slice.LastIndexOf("`n")
      if ($lastNl -ge 0 -and $lastNl -gt [int]($len*0.5)) { $len=$lastNl+1 }
    }
    $chunks.Add($text.Substring($start,$len)) | Out-Null; $start+=$len
  }
  return $chunks
}

function Take-PrefixChunk([string]$text, [int]$cap) {
  if ($cap -le 0) { return @("", $text) }
  $len=[Math]::Min($cap,$text.Length); $slice=$text.Substring(0,$len)
  $lastNl=$slice.LastIndexOf("`n")
  if ($lastNl -ge 0 -and $lastNl -gt [int]($len*0.5)) { $len=$lastNl+1 }
  $first=$text.Substring(0,$len)
  $rest = if ($len -lt $text.Length) { $text.Substring($len) } else { "" }
  return @($first, $rest)
}

# Dateien einsammeln
$all = Get-ChildItem -Path $Root -Recurse -File | Where-Object {
  $ext=$_.Extension.ToLower()
  ($Extensions -contains $ext) -and -not (Path-IsExcluded $_.FullName $ExcludeSubpaths)
} | Sort-Object FullName
if (-not $all) { throw "Keine passenden Dateien gefunden." }

# Vorverarbeitung
$files=@()
foreach ($f in $all) {
  $raw=Get-Content -LiteralPath $f.FullName -Raw
  $stripped=Strip-CStyleComments $raw
  $files += [pscustomobject]@{
    FullPath=$f.FullName; RelPath=Get-RelativePath -basePath $Root -fullPath $f.FullName
    Code=$stripped; CodeLen=$stripped.Length
  }
}

# Bundling
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$bundleIndex=1; $bundleCodeLen=0
$bundleFiles=New-Object System.Collections.Generic.List[object]
$bundleSb=New-Object System.Text.StringBuilder

function New-BundleName([int]$idx){ return ("bundle_{0:000}.txt" -f $idx) }
function New-MetaName([int]$idx){ return ("bundle_{0:000}.meta.json" -f $idx) }
function Normalize-Newlines([string]$s){ return ($s -replace '\r\n?', "`n") }
function Extract-CodeFromBundleText([string]$text){
  $pat='(?ms)^/\*\s*===\s*BEGIN\s*FILE:.*?\*/\r?\n(.*?)(?=\r?\n/\*\s*===\s*END\s*FILE:.*?\*/\r?\n?)'
  $m=[regex]::Matches($text,$pat); if ($m.Count -eq 0){ return "" }
  return (($m | ForEach-Object { $_.Groups[1].Value }) -join "")
}

function Flush-Bundle(){
  if ($bundleFiles.Count -eq 0) { return }
  $name=Join-Path $OutDir (New-BundleName $bundleIndex)
  $meta=Join-Path $OutDir (New-MetaName $bundleIndex)
  $bundleText=$bundleSb.ToString()

  $codeFromText=Normalize-Newlines (Extract-CodeFromBundleText $bundleText)
  $codeFromMem =Normalize-Newlines ( ($bundleFiles | ForEach-Object { $_.Code }) -join "" )
  if ($codeFromText.Length -ne $codeFromMem.Length -or $codeFromText -cne $codeFromMem) {
    throw ("Verifikation fehlgeschlagen für {0}: erwartet {1}, gefunden {2}" -f $name, $codeFromMem.Length, $codeFromText.Length)
  }

  [IO.File]::WriteAllText($name,$bundleText,[Text.UTF8Encoding]::new($false))

  $filesForMeta=@($bundleFiles | ForEach-Object { [pscustomobject]@{ rel=$_.RelPath; code_len=$_.CodeLen } })
  $metaObj=[pscustomobject]@{ bundle=[IO.Path]::GetFileName($name); total_code_len=$codeFromMem.Length; files=$filesForMeta }
  $metaJson=$metaObj | ConvertTo-Json -Depth 6
  [IO.File]::WriteAllText($meta,$metaJson,[Text.UTF8Encoding]::new($false))

  $script:grandTotal += $codeFromMem.Length

  $script:bundleIndex++; $script:bundleCodeLen=0; $script:bundleFiles.Clear(); $null=$script:bundleSb.Clear()
}

foreach ($item in $files) {
  if ($item.CodeLen -le $MaxChars) {
    if ($bundleCodeLen -gt 0 -and ($bundleCodeLen + $item.CodeLen) -gt $MaxChars) { Flush-Bundle }
    $header="/* === BEGIN FILE: $($item.RelPath) === */`r`n"
    $footer="`r`n/* === END FILE: $($item.RelPath) === */`r`n"
    [void]$bundleSb.Append($header); [void]$bundleSb.Append($item.Code); [void]$bundleSb.Append($footer)
    $bundleCodeLen += $item.CodeLen; $bundleFiles.Add($item) | Out-Null
  }
  else {
    if ($bundleCodeLen -gt 0) {
      # Restplatz mit Part 1 füllen
      $remaining = $MaxChars - $bundleCodeLen
      $split = Take-PrefixChunk $item.Code $remaining
      $part1 = $split[0]; $rest = $split[1]
      $restChunks = @(); if ($rest.Length -gt 0) { $restChunks = @(Split-IntoChunks $rest $MaxChars) }
      $restCount  = @($restChunks).Count
      $totalParts = 1 + $restCount

      if ($part1.Length -gt 0) {
        $part1Obj=[pscustomobject]@{ FullPath=$item.FullPath; RelPath="$($item.RelPath) [part 1/$totalParts]"; Code=$part1; CodeLen=$part1.Length }
        $header="/* === BEGIN FILE: $($part1Obj.RelPath) === */`r`n"; $footer="`r`n/* === END FILE: $($part1Obj.RelPath) === */`r`n"
        [void]$bundleSb.Append($header); [void]$bundleSb.Append($part1Obj.Code); [void]$bundleSb.Append($footer)
        $bundleCodeLen += $part1Obj.CodeLen; $bundleFiles.Add($part1Obj) | Out-Null
        Flush-Bundle
      }

      for ($pi=0; $pi -lt $restCount; $pi++) {
        $idx = $pi + 2; $isLast = ($pi -eq $restCount - 1)
        $partCode = $restChunks[$pi]
        $partObj=[pscustomobject]@{ FullPath=$item.FullPath; RelPath="$($item.RelPath) [part $idx/$totalParts]"; Code=$partCode; CodeLen=$partCode.Length }
        $header="/* === BEGIN FILE: $($partObj.RelPath) === */`r`n"; $footer="`r`n/* === END FILE: $($partObj.RelPath) === */`r`n"
        [void]$bundleSb.Append($header); [void]$bundleSb.Append($partObj.Code); [void]$bundleSb.Append($footer)
        $bundleCodeLen = $partObj.CodeLen; $bundleFiles.Clear(); $bundleFiles.Add($partObj) | Out-Null
        if (-not $isLast) { Flush-Bundle }
      }
    }
    else {
      # Bundle leer: Datei in Chunks, letzter Part offen
      $chunks = @(Split-IntoChunks $item.Code $MaxChars)
      $count  = @($chunks).Count
      for ($pi=0; $pi -lt $count; $pi++) {
        $isLast = ($pi -eq $count - 1); $idx=$pi+1
        $partCode = $chunks[$pi]
        $partObj=[pscustomobject]@{ FullPath=$item.FullPath; RelPath="$($item.RelPath) [part $idx/$count]"; Code=$partCode; CodeLen=$partCode.Length }
        $header="/* === BEGIN FILE: $($partObj.RelPath) === */`r`n"; $footer="`r`n/* === END FILE: $($partObj.RelPath) === */`r`n"
        [void]$bundleSb.Append($header); [void]$bundleSb.Append($partObj.Code); [void]$bundleSb.Append($footer)
        $bundleCodeLen = $partObj.CodeLen; $bundleFiles.Clear(); $bundleFiles.Add($partObj) | Out-Null
        if (-not $isLast) { Flush-Bundle }
      }
    }
  }
}

Flush-Bundle
Write-Host ("Gesamtzeichen aller Bundles: {0}" -f $script:grandTotal)
Write-Host "Fertig. Bundles in: $OutDir"

