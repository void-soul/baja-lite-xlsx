# 本地测试 GitHub Actions 的 PowerShell 脚本
# 用于验证脚本在推送到 GitHub 前是否正确

Write-Host "==================================="
Write-Host "测试 GitHub Actions PowerShell 脚本"
Write-Host "==================================="
Write-Host ""

# 检查 prebuilds 目录
if (-not (Test-Path "prebuilds")) {
    Write-Host "错误: prebuilds 目录不存在"
    Write-Host "请先运行: .\scripts\create-prebuilds.bat"
    exit 1
}

Write-Host "步骤 1: List generated files"
Write-Host "-----------------------------------"
Get-ChildItem prebuilds -Recurse -File | Select-Object Name, @{Name="Size(KB)";Expression={[math]::Round($_.Length/1KB, 1)}}
Write-Host ""

Write-Host "步骤 2: Verify package contents"
Write-Host "-----------------------------------"
$tarFiles = Get-ChildItem prebuilds -Filter *.tar.gz
if ($tarFiles.Count -gt 0) {
    $firstFile = $tarFiles[0]
    Write-Host "Extracting $($firstFile.Name) to verify contents..."
    $extractPath = "temp_verify"
    New-Item -ItemType Directory -Path $extractPath -Force | Out-Null
    tar -xzf $firstFile.FullName -C $extractPath
    Write-Host "Package contents:"
    Get-ChildItem $extractPath -Recurse -File | Select-Object Name, @{Name="Size(KB)";Expression={[math]::Round($_.Length/1KB, 1)}}
    $dllFiles = Get-ChildItem $extractPath -Recurse -Filter "*.dll"
    Write-Host "`nFound $($dllFiles.Count) DLL files"
    $dllFiles | ForEach-Object { Write-Host "  - $($_.Name)" }
    
    # 验证关键 DLL
    Write-Host "`n验证关键 DLL:"
    $hasXlnt = ($dllFiles | Where-Object { $_.Name -eq "xlnt.dll" }).Count -gt 0
    $hasZlib = ($dllFiles | Where-Object { $_.Name -like "zlib*.dll" }).Count -gt 0
    
    if ($hasXlnt) {
        Write-Host "  ✓ xlnt.dll 存在"
    } else {
        Write-Host "  ✗ xlnt.dll 缺失"
    }
    
    if ($hasZlib) {
        Write-Host "  ✓ zlib DLL 存在"
    } else {
        Write-Host "  ✗ zlib DLL 缺失"
    }
    
    Write-Host ""
    if ($hasXlnt -and $hasZlib) {
        Write-Host "✅ 验证通过 - 预编译包包含所有必需的 DLL"
    } else {
        Write-Host "❌ 验证失败 - 预编译包缺少必需的 DLL"
    }
    
    Remove-Item $extractPath -Recurse -Force
} else {
    Write-Host "错误: 没有找到 .tar.gz 文件"
    exit 1
}

Write-Host ""
Write-Host "==================================="
Write-Host "测试完成"
Write-Host "==================================="

