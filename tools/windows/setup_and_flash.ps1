<#
  Gom toàn bộ project ESP32 vào C:\Users\Asus\Documents\Arduino\esp32-projects rồi nạp vào ESP-Mosaico V1.2.

  Cách chạy (PowerShell):
    cd C:\Users\Asus\Documents\Arduino
    powershell -ExecutionPolicy Bypass -File .\setup_and_flash.ps1                 # gom + nạp self-test
    powershell -ExecutionPolicy Bypass -File .\setup_and_flash.ps1 -Flash firmware # gom + nạp firmware chính
    powershell -ExecutionPolicy Bypass -File .\setup_and_flash.ps1 -Flash none     # chỉ gom, không nạp
  Thêm -Port COM5 nếu có nhiều cổng COM.

  Cần: Git for Windows, Python 3. Lần đầu nạp: tắt máy -> giữ BOOT -> nhấn POWER rồi mới chạy.
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
python -m pip install --quiet --upgrade esptool pyserial
$File = if ($Flash -eq "hwtest") { $HwBin } else { $FwBin }
if (-not (Test-Path $File)) { Fail "Không có file $File" }

if (-not $Port) {
    $ports = [System.IO.Ports.SerialPort]::GetPortNames()
    if ($ports.Count -eq 0) { Fail "Không thấy cổng COM. Cắm cáp USB-C có data, tắt máy -> giữ BOOT -> nhấn POWER rồi chạy lại." }
    if ($ports.Count -gt 1) { Fail "Có nhiều cổng: $($ports -join ', '). Chạy lại với -Port COMx." }
    $Port = $ports[0]
}
Ok "Nạp $File vào $Port"
python -m esptool --port $Port --baud 921600 write_flash 0x0 $File
if ($LASTEXITCODE -ne 0) { Fail "Nạp lỗi. Thử lại sau khi: tắt máy -> giữ BOOT -> nhấn POWER." }
Ok "Nạp xong. Bấm POWER (hoặc RESET) để chạy."

if ($Flash -eq "hwtest") {
    Write-Host "Mở log self-test (Ctrl+] để thoát). Kết quả: 1 rung dài = PASS, 3 rung ngắn = lỗi."
    Start-Sleep -Seconds 3
    python -m serial.tools.miniterm $Port 115200
} else {
    Write-Host "Máy phát Wi-Fi 'esp-claw-xxxx': kết nối vào, trang cài đặt tự mở (hoặc http://192.168.4.1)."
}
