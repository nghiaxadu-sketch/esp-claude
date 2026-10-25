<#
  Gom toàn bộ project ESP32 vào C:\Users\Asus\Documents\Arduino\esp32-projects rồi nạp vào ESP-Mosaico V1.2.

  Cách chạy (PowerShell):
    cd C:\Users\Asus\Documents\Arduino
    powershell -ExecutionPolicy Bypass -File .\setup_and_flash.ps1                 # gom + nạp self-test
    powershell -ExecutionPolicy Bypass -File .\setup_and_flash.ps1 -Flash firmware # gom + nạp firmware chính
    powershell -ExecutionPolicy Bypass -File .\setup_and_flash.ps1 -Flash none     # chỉ gom, không nạp
  Thêm -Port COM5 nếu có nhiều cổng COM.

  Cần: Git for Windows, Python 3.10-3.13 (khuyên 3.12; 3.14+ chưa có wheel PyYAML). Lần đầu nạp: tắt máy -> giữ BOOT -> nhấn POWER rồi mới chạy.
#>
param(
    [ValidateSet("hwtest", "firmware", "none")]
    [string]$Flash = "hwtest",
    [string]$Port = "",
    [string]$Root = "C:\Users\Asus\Documents\Arduino\esp32-projects",
    [string]$Branch = "claude/vigilant-lovelace-cdg7mx"
)

$ErrorActionPreference = "Stop"
$Board = "esp_mosaico_v1_2"
$FirmwareUrl = "https://github.com/nghiaxadu-sketch/esp-claude/releases/download/esp-claude-latest/esp-claude-$Board.bin"

function Ok($m)   { Write-Host "✔ $m" -ForegroundColor Green }
function Warn($m) { Write-Host "⚠ $m" -ForegroundColor Yellow }
function Fail($m) { Write-Host "✖ $m" -ForegroundColor Red; exit 1 }

function Need($cmd, $hint) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) { Fail "Chưa có '$cmd'. $hint" }
}

# Clone lần đầu, các lần sau chỉ pull bản mới nhất.
function Sync-Repo($url, $dir) {
    if (Test-Path (Join-Path $dir ".git")) {
        git -C $dir fetch origin $Branch
        git -C $dir checkout $Branch
        git -C $dir pull --ff-only origin $Branch
        Ok "Cập nhật $dir"
    } else {
        git clone --branch $Branch $url $dir
        Ok "Tải về $dir"
    }
}

Need git    "Cài Git for Windows: https://git-scm.com/download/win"
Need python "Cài Python 3: https://www.python.org/downloads/ (tick 'Add to PATH')"

New-Item -ItemType Directory -Force -Path $Root | Out-Null
$Src = Join-Path $Root "_src"
New-Item -ItemType Directory -Force -Path $Src | Out-Null
$Bin = Join-Path $Root "bin"
New-Item -ItemType Directory -Force -Path $Bin | Out-Null

# 1. Source code
Sync-Repo "https://github.com/nghiaxadu-sketch/esp-claude.git" (Join-Path $Root "esp-claude")
Sync-Repo "https://github.com/nghiaxadu-sketch/claudecode.git" (Join-Path $Src "claudecode")   # repo private: Git sẽ hỏi đăng nhập GitHub lần đầu
robocopy (Join-Path $Src "claudecode\esp32-projects\esp_mosaico_hwtest") (Join-Path $Root "esp_mosaico_hwtest") /MIR /NFL /NDL /NJH /NJS | Out-Null
Copy-Item (Join-Path $Src "claudecode\esp32-projects\README.md") (Join-Path $Root "README.md") -Force
Ok "Đã gom code vào $Root"

# 2. File nạp
$FwBin = Join-Path $Bin "esp-claude-$Board.bin"
Invoke-WebRequest -Uri $FirmwareUrl -OutFile $FwBin
Ok "Tải firmware: $FwBin"

$HwBin = Join-Path $Bin "esp-mosaico-hwtest.bin"
if (Get-Command gh -ErrorAction SilentlyContinue) {
    gh release download mosaico-hwtest-latest -R nghiaxadu-sketch/claudecode -p "esp-mosaico-hwtest.bin" -D $Bin --clobber
    Ok "Tải self-test: $HwBin"
} elseif (-not (Test-Path $HwBin)) {
    Warn "Repo claudecode là private nên cần GitHub CLI (gh) để tải self-test tự động."
    Warn "Hoặc tải tay: https://github.com/nghiaxadu-sketch/claudecode/releases/tag/mosaico-hwtest-latest -> lưu vào $Bin"
}

if ($Flash -eq "none") { Ok "Xong (không nạp)."; exit 0 }

# 3. Nạp
# Python quá mới (3.14+) chưa có wheel PyYAML -> pip đòi MSVC. Ưu tiên 3.13/3.12/3.11/3.10 qua py launcher, cài vào venv riêng.
function Find-Py {
    foreach ($v in "3.13", "3.12", "3.11", "3.10") {
        if (Get-Command py -ErrorAction SilentlyContinue) {
            $prev = $ErrorActionPreference; $ErrorActionPreference = "Continue"
            py "-$v" -c "import sys" 2>&1 | Out-Null
            $code = $LASTEXITCODE; $ErrorActionPreference = $prev
            if ($code -eq 0) { return @("py", "-$v") }
        }
    }
    return $null
}
$BasePy = Find-Py
if (-not $BasePy -and (Get-Command winget -ErrorAction SilentlyContinue)) {
    Warn "Chưa có Python 3.10-3.13, tự cài Python 3.12 bằng winget..."
    winget install -e --id Python.Python.3.12 --scope user --accept-package-agreements --accept-source-agreements
    $env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")
    $BasePy = Find-Py
}
if (-not $BasePy) {
    Fail "Cần Python 3.10-3.13 (khuyên 3.12): https://www.python.org/downloads/release/python-3120/ rồi chạy lại. Python 3.14+ chưa có wheel PyYAML."
}
$Venv = Join-Path $Root ".venv"
$VenvPy = Join-Path $Venv "Scripts\python.exe"
if (-not (Test-Path $VenvPy)) {
    & $BasePy[0] $BasePy[1..($BasePy.Count)] -m venv $Venv
    if ($LASTEXITCODE -ne 0) { Fail "Không tạo được venv $Venv" }
}
& $VenvPy -m pip install --quiet --prefer-binary --upgrade esptool pyserial
if ($LASTEXITCODE -ne 0) { Fail "Cài esptool lỗi. Cài Python 3.12 (https://www.python.org/downloads/release/python-3120/) rồi xoá thư mục $Venv và chạy lại." }
$File = if ($Flash -eq "hwtest") { $HwBin } else { $FwBin }
if (-not (Test-Path $File)) { Fail "Không có file $File" }

if (-not $Port) {
    $ports = @(Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '\((COM\d+)\)' } |
        ForEach-Object { [pscustomobject]@{ Port = $Matches[1]; Name = $_.Name } })
    if ($ports.Count -eq 0) { Fail "Không thấy cổng COM. Cắm cáp USB-C có data, tắt máy -> giữ BOOT -> nhấn POWER rồi chạy lại." }
    if ($ports.Count -eq 1) {
        $Port = $ports[0].Port
    } else {
        Warn "Có nhiều cổng COM:"
        $ports | ForEach-Object { Write-Host "   $($_.Name)" }
        $Port = (Read-Host "Nhập cổng của ESP32 (vd COM3)").Trim().ToUpper()
        if ($Port -notmatch '^COM\d+$') { Fail "Cổng không hợp lệ: '$Port'. Chạy lại với -Port COMx." }
    }
}
Ok "Nạp $File vào $Port"
& $VenvPy -m esptool --port $Port --baud 921600 write_flash 0x0 $File
if ($LASTEXITCODE -ne 0) { Fail "Nạp lỗi. Thử lại sau khi: tắt máy -> giữ BOOT -> nhấn POWER." }
Ok "Nạp xong. Bấm POWER (hoặc RESET) để chạy."

if ($Flash -eq "hwtest") {
    Write-Host "Mở log self-test (Ctrl+] để thoát). Kết quả: 1 rung dài = PASS, 3 rung ngắn = lỗi."
    Start-Sleep -Seconds 3
    & $VenvPy -m serial.tools.miniterm $Port 115200
} else {
    Write-Host "Máy phát Wi-Fi 'esp-claw-xxxx': kết nối vào, trang cài đặt tự mở (hoặc http://192.168.4.1)."
}
