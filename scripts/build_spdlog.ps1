# build_spdlog.ps1 — 预构建并 install spdlog 到项目 build 目录
# 用途: 为 CommonLibSSE-NG 提供 find_package(spdlog CONFIG) 可用配置
# 用法: powershell -File scripts/build_spdlog.ps1
$ErrorActionPreference = 'Stop'
$bt = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools"
$vcvars = "$bt\VC\Auxiliary\Build\vcvars64.bat"
$cmake = "$bt\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

$Root   = Split-Path -Parent $PSScriptRoot
$Src    = Join-Path $Root 'thirdparty\spdlog'
$Build  = Join-Path $Root 'build\deps\spdlog-build'
$Prefix = Join-Path $Root 'build\deps\spdlog'

if (-not (Test-Path "$Src\CMakeLists.txt")) { throw "spdlog source not found: $Src" }

# 进 vcvars 环境(只用于确保有 MSVC; 配置/构建用 VS generator, 实际继承)
$env:VCPATH = $null
& cmd /c "`"$vcvars`" >nul 2>&1"

Write-Host "Configuring spdlog (VS2022/v145, Release) ..."
& $cmake -S $Src -B $Build -G "Visual Studio 17 2022" -A x64 -T v145 `
	"-DSPDLOG_BUILD_TESTS=OFF" "-DSPDLOG_BUILD_EXAMPLE=OFF" "-DSPDLOG_INSTALL=ON" `
	"-DCMAKE_INSTALL_PREFIX=$Prefix"
if ($LASTEXITCODE -ne 0) { throw "spdlog configure failed" }

Write-Host "Building spdlog ..."
& $cmake --build $Build --config Release
if ($LASTEXITCODE -ne 0) { throw "spdlog build failed" }

Write-Host "Installing spdlog into $Prefix ..."
& $cmake --install $Build --config Release "--prefix=$Prefix"
if ($LASTEXITCODE -ne 0) { throw "spdlog install failed" }

if (Test-Path "$Prefix\lib\cmake\spdlog\spdlogConfig.cmake") {
	Write-Host "OK: spdlog staged at $Prefix"
} else {
	throw "spdlogConfig.cmake not found after install"
}
