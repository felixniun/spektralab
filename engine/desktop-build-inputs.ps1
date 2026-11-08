function Get-DesktopBuildInputs([string]$Repository) {
    $desktop = Join-Path $Repository 'desktop'
    $files = @(
        Get-Item -LiteralPath (Join-Path $Repository 'modern_UI\Spektrafilm\Spektrafilm\Resources\StockCatalog.json')
        Get-ChildItem -LiteralPath (Join-Path $desktop 'src'), (Join-Path $desktop 'public'), (Join-Path $desktop 'src-tauri\src') -File -Recurse
        foreach ($name in @('package.json', 'package-lock.json', 'index.html', 'vite.config.ts', 'src-tauri\Cargo.toml', 'src-tauri\Cargo.lock', 'src-tauri\tauri.conf.json', 'src-tauri\build.rs')) {
            Get-Item -LiteralPath (Join-Path $desktop $name)
        }
    )
    $hashes = [ordered]@{}
    foreach ($file in ($files | Sort-Object FullName)) {
        $relative = [IO.Path]::GetRelativePath($Repository, $file.FullName).Replace('\', '/')
        $hashes[$relative] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $hashes
}
