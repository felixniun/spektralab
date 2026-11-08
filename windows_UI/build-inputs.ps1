function Get-QtBuildInputs([string]$Repository) {
    $paths=@('CMakeLists.txt','windows_UI/CMakeLists.txt','windows_UI/src','windows_UI/qml',
        'windows_UI/dependencies','engine/CMakeLists.txt','engine/src','engine/include',
        'engine/cmake','engine/third_party','engine/resources','engine/resources_product',
        'engine/host/white_balance.cpp','engine/host/white_balance.hpp',
        'modern_UI/Spektrafilm/Spektrafilm/Resources/FilmCovers')
    $inputs=[ordered]@{}
    foreach($file in (Get-ChildItem -LiteralPath @($paths | ForEach-Object {Join-Path $Repository $_}) -Recurse -File | Sort-Object FullName)) {
        $inputs[[IO.Path]::GetRelativePath($Repository,$file.FullName).Replace('\','/')]=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $inputs
}
