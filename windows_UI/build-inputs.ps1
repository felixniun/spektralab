function Get-QtBuildInputs([string]$Repository) {
    $paths=@('CMakeLists.txt','windows_UI/CMakeLists.txt','windows_UI/src','windows_UI/qml',
        'windows_UI/dependencies','engine/CMakeLists.txt','engine/src','engine/include',
        'engine/cmake','engine/third_party','engine/resources','engine/resources_product',
        'engine/host/white_balance.cpp','engine/host/white_balance.hpp',
        'engine/host/image_io.cpp','engine/host/image_io.hpp','engine/host/platform.cpp','engine/host/platform.hpp',
        'engine/host/stb_impl.cpp','engine/host/tiff_ifd.cpp','engine/host/tiff_ifd.hpp',
        'modern_UI/Spektrafilm/Spektrafilm/Resources/FilmCovers')
    $inputs=[ordered]@{}
    foreach($file in (Get-ChildItem -LiteralPath @($paths | ForEach-Object {Join-Path $Repository $_}) -Recurse -File | Sort-Object FullName)) {
        $inputs[[IO.Path]::GetRelativePath($Repository,$file.FullName).Replace('\','/')]=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $inputs
}
