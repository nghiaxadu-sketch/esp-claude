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
if (-not $BasePy -and (Get-Command py -ErrorAction SilentlyContinue)) {
    # Python install manager mới (py install ...) — máy đang có 3.15 qua công cụ này.
    Warn "Chưa có Python 3.10-3.13, tự cài Python 3.12 bằng 'py install 3.12'..."
    $prev = $ErrorActionPreference; $ErrorActionPreference = "Continue"
    py install 3.12
    $ErrorActionPreference = $prev
    $BasePy = Find-Py
}
if (-not $BasePy -and (Get-Command winget -ErrorAction SilentlyContinue)) {
    Warn "Tự cài Python 3.12 bằng winget..."
    $prev = $ErrorActionPreference; $ErrorActionPreference = "Continue"
    winget install -e --id Python.Python.3.12 --scope user --accept-package-agreements --accept-source-agreements
    $ErrorActionPreference = $prev
    $env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")
    $BasePy = Find-Py
}
if (-not $BasePy) {
    Fail "Cần Python 3.10-3.13 (khuyên 3.12). Chạy 'py install 3.12' hoặc tải https://www.python.org/downloads/release/python-3120/ rồi chạy lại."
}
Ok "Dùng Python $($BasePy -join ' ')"
$Venv = Join-Path $Root ".venv"
$VenvPy = Join-Path $Venv "Scripts\python.exe"
# venv cũ tạo bằng Python 3.14+ thì tạo lại
if (Test-Path $VenvPy) {
    $minor = & $VenvPy -c "import sys; print(sys.version_info[1])"
    if ([int]$minor -ge 14 -or [int]$minor -lt 10) { Remove-Item $Venv -Recurse -Force }
}
if (-not (Test-Path $VenvPy)) {
    & $BasePy[0] $BasePy[1] -m venv $Venv
    if ($LASTEXITCODE -ne 0) { Fail "Không tạo được venv $Venv" }
}
$prev = $ErrorActionPreference; $ErrorActionPreference = "Continue"
& $VenvPy -m pip install --quiet --disable-pip-version-check --only-binary pyyaml --upgrade esptool pyserial
$code = $LASTEXITCODE; $ErrorActionPreference = $prev
if ($code -ne 0) { Fail "Cài esptool lỗi. Xoá thư mục $Venv rồi chạy lại." }
Ok "Đã cài esptool"
$File = if ($Flash -eq "hwtest") { $HwBin } else { $FwBin }
if (-not (Test-Path $File)) { Fail "Không có file $File" }

# Cổng ESP32: ưu tiên VID Espressif 303A. ROM của ESP32-S31 đổi cổng khi vào chế độ nạp nên tìm lại mỗi lần thử.
function Find-Port {
    $prev = $ErrorActionPreference; $ErrorActionPreference = "Continue"
    $list = @(& $VenvPy -c "import serial.tools.list_ports as l; [print(p.device, '%04X' % (p.vid or 0), p.description) for p in l.comports()]")
    $ErrorActionPreference = $prev
    $esp = @($list | Where-Object { ($_ -split ' ')[1] -eq '303A' })
    if ($esp.Count -ge 1) { return ($esp[0] -split ' ')[0] }
    if ($list.Count -eq 1) { return ($list[0] -split ' ')[0] }
    if ($list.Count -gt 1) {
        Warn "Có nhiều cổng COM:"
        $list | ForEach-Object { Write-Host "   $_" }
        $sel = (Read-Host "Nhập cổng của ESP32 (vd COM3)").Trim().ToUpper()
        if ($sel -notmatch '^COM\d+$') { Fail "Cổng không hợp lệ: '$sel'. Chạy lại với -Port COMx." }
        return $sel
    }
    return $null
}

$ok = $false
for ($try = 1; $try -le 3; $try++) {
    $p = if ($Port) { $Port } else { Find-Port }
    if (-not $p) {
        Warn "Lần $($try): chưa thấy máy. Tắt máy -> giữ BOOT -> nhấn POWER -> thả BOOT (cáp USB-C có data)."
        Start-Sleep -Seconds 5
        continue
    }
    Ok "Lần $($try): nạp $File vào $p"
    # Không reset qua DTR/RTS: cổng USB của ROM S31 rớt kết nối ("device not functioning") nếu bị reset từ máy tính.
    $prev = $ErrorActionPreference; $ErrorActionPreference = "Continue"
    & $VenvPy -m esptool --port $p --baud 460800 --before no_reset --after no_reset write_flash 0x0 $File
    $code = $LASTEXITCODE; $ErrorActionPreference = $prev
    if ($code -eq 0) { $ok = $true; $Port = $p; break }
    Warn "Không mở được $p. Đóng Arduino IDE / Serial Monitor / tab esptool-js nếu đang mở; thử lại sau 4 giây..."
    Start-Sleep -Seconds 4
}
if (-not $ok) { Fail "Nạp lỗi 3 lần. Rút cáp, cắm lại, tắt máy -> giữ BOOT -> nhấn POWER rồi chạy lại." }
Ok "Nạp xong. Bấm POWER để khởi động lại máy."

if ($Flash -eq "hwtest") {
    Write-Host "Mở log self-test (Ctrl+] để thoát). Kết quả: 1 rung dài = PASS, 3 rung ngắn = lỗi."
    Read-Host "Bấm POWER cho máy chạy, rồi nhấn Enter ở đây để mở log"
    $logPort = Find-Port
    if ($logPort) { & $VenvPy -m serial.tools.miniterm $logPort 115200 } else { Warn "Không thấy cổng log; xem kết quả bằng motor rung." }
} else {
    Write-Host "Máy phát Wi-Fi 'esp-claw-xxxx': kết nối vào, trang cài đặt tự mở (hoặc http://192.168.4.1)."
}
