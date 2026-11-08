function Sync-ProductResources([string]$Repository, [string]$Destination) {
    # Same overlay as engine/build.sh bundle. Never modify resources/ itself.
    $product = Join-Path $Repository 'engine\resources_product'
    New-Item -ItemType Directory -Force -Path (Join-Path $Destination 'profiles') | Out-Null
    Get-ChildItem -LiteralPath (Join-Path $product 'profiles') -Filter '*.json' -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $Destination 'profiles') -Force
    }
    Copy-Item -LiteralPath (Join-Path $product 'NOTICE.md') -Destination (Join-Path $Destination 'PRODUCT_PROFILES_NOTICE.md') -Force
    Assert-CatalogResources $Repository $Destination
}

function Assert-CatalogResources([string]$Repository, [string]$Resources) {
    $catalog = Get-Content -LiteralPath (Join-Path $Repository 'modern_UI\Spektrafilm\Spektrafilm\Resources\StockCatalog.json') -Raw | ConvertFrom-Json
    foreach ($stock in $catalog.stocks) {
        $path = Join-Path $Resources "profiles\$($stock.id).json"
        if (-not (Test-Path -LiteralPath $path)) { throw "Catalogue profile missing from runtime resources: $($stock.id). Run Sync-ProductResources before packaging." }
        $profile = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        if ($profile.info.stock -ne $stock.id) { throw "Catalogue/profile ID mismatch: $path" }
        $reference = Join-Path $Repository "engine\resources_product\profiles\$($stock.id).json"
        if (-not (Test-Path -LiteralPath $reference)) { $reference = Join-Path $Repository "engine\resources\profiles\$($stock.id).json" }
        if ((Get-FileHash -LiteralPath $reference -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash) { throw "Runtime profile differs from project source: $($stock.id)" }
    }
}
