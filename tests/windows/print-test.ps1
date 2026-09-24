<#
Deterministic P1007 test pages, drawn in device pixels via System.Drawing.Printing.
There's no PDF viewer or app in the path, so there's no scaling, headers, or anti-aliasing.
Coordinates are device pixels relative to the printable-area origin.

Examples:
  .\print-test.ps1 -Test blank
  .\print-test.ps1 -Test pixel -X 600 -Y 600
  .\print-test.ps1 -Test rect  -X 600 -Y 600 -W 100 -H 100
  .\print-test.ps1 -Test checker -X 600 -Y 600 -W 64 -H 64 -Cell 8
  .\print-test.ps1 -Test blank -ToFile C:\p1007\blank.prn   # driver output without USB
#>
param(
  [ValidateSet('blank','pixel','rect','checker','hstripes','vstripes')] [string]$Test = 'blank',
  [string]$Printer = 'HP LaserJet P1007',
  [int]$X = 600, [int]$Y = 600, [int]$W = 100, [int]$H = 100, [int]$Cell = 8,
  [string]$Paper = 'A4',
  [int]$Dpi = 600,
  [string]$ToFile = ''
)
Add-Type -AssemblyName System.Drawing

$doc = New-Object System.Drawing.Printing.PrintDocument
$doc.DocumentName = "p1007-$Test"
$doc.PrinterSettings.PrinterName = $Printer
if (-not $doc.PrinterSettings.IsValid) { throw "Printer '$Printer' not found" }
if ($ToFile) { $doc.PrinterSettings.PrintToFile = $true; $doc.PrinterSettings.PrintFileName = $ToFile }

$ps = $doc.PrinterSettings.PaperSizes | Where-Object { $_.PaperName -like "$Paper*" } | Select-Object -First 1
if ($ps) { $doc.DefaultPageSettings.PaperSize = $ps }
$res = $doc.PrinterSettings.PrinterResolutions | Where-Object { $_.X -eq $Dpi } | Select-Object -First 1
if ($res) { $doc.DefaultPageSettings.PrinterResolution = $res }
$doc.DefaultPageSettings.Landscape = $false
$doc.PrinterSettings.Copies = 1

$doc.add_PrintPage({
  param($s, $e)
  $g = $e.Graphics
  $g.PageUnit = [System.Drawing.GraphicsUnit]::Pixel
  $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::None
  $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::None
  $b = [System.Drawing.Brushes]::Black
  switch ($Test) {
    'blank'    { }
    'pixel'    { $g.FillRectangle($b, $X, $Y, 1, 1) }
    'rect'     { $g.FillRectangle($b, $X, $Y, $W, $H) }
    'checker'  { for ($j=0; $j -lt $H; $j+=$Cell) { for ($i=0; $i -lt $W; $i+=$Cell) {
                   if ((($i/$Cell) + ($j/$Cell)) % 2 -eq 0) { $g.FillRectangle($b, $X+$i, $Y+$j, $Cell, $Cell) } } } }
    'hstripes' { for ($j=0; $j -lt $H; $j+=2*$Cell) { $g.FillRectangle($b, $X, $Y+$j, $W, $Cell) } }
    'vstripes' { for ($i=0; $i -lt $W; $i+=2*$Cell) { $g.FillRectangle($b, $X+$i, $Y, $Cell, $H) } }
  }
  $e.HasMorePages = $false
})

# Sidecar metadata, so every capture records exactly what was requested
$meta = [ordered]@{
  test=$Test; printer=$Printer; x=$X; y=$Y; w=$W; h=$H; cell=$Cell
  paper=$doc.DefaultPageSettings.PaperSize.PaperName
  dpi="$($doc.DefaultPageSettings.PrinterResolution.X)x$($doc.DefaultPageSettings.PrinterResolution.Y)"
  hardMarginX=$doc.DefaultPageSettings.HardMarginX; hardMarginY=$doc.DefaultPageSettings.HardMarginY
  toFile=$ToFile; utc=(Get-Date).ToUniversalTime().ToString('o')
}
$meta | ConvertTo-Json | Out-File -Encoding utf8 "p1007-$Test-$((Get-Date).ToString('yyyyMMdd-HHmmss')).json"
$doc.Print()
$meta | ConvertTo-Json
