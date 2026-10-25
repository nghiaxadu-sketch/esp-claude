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

# ---------- Tìm cổng ----------
# ESP-Mosaico cắm Type-C trực tiếp -> cổng VID 303A (Espressif). Cổng CH340/CP210x/FTDI chỉ đúng khi
# nối dây USB-UART vào pad TX/RX/GND mặt sau. Cổng Bluetooth luôn bỏ qua.
function Get-Ports {
    $prev = $ErrorActionPreference; $ErrorActionPreference = "Continue"
    $rows = @(& $VenvPy -c "import serial.tools.list_ports as l; [print('%s|%04X|%s' % (p.device, p.vid or 0, p.description)) for p in l.comports()]")
    $ErrorActionPreference = $prev
    @($rows | ForEach-Object {
        $f = $_ -split '\|', 3
        [pscustomobject]@{ Port = $f[0]; Vid = $f[1]; Name = $f[2] }
    } | Where-Object { $_.Vid -ne '0000' -and $_.Name -notmatch 'Bluetooth' })
}

function Select-Port {
    if ($Port) { return [pscustomobject]@{ Port = $Port; Vid = 'USER'; Name = $Port } }
    $ports = Get-Ports
    $esp = @($ports | Where-Object { $_.Vid -eq '303A' })
    if ($esp.Count -ge 1) { return $esp[0] }
    if ($ports.Count -eq 0) { return $null }
    if ($ports.Count -eq 1) { $c = $ports[0] } else {
        Warn "Có nhiều cổng USB-serial:"
        $ports | ForEach-Object { Write-Host "   $($_.Port)  $($_.Name)" }
        $sel = (Read-Host "Nhập cổng của ESP-Mosaico (vd COM5)").Trim().ToUpper()
        $c = $ports | Where-Object { $_.Port -eq $sel } | Select-Object -First 1
        if (-not $c) { Fail "Cổng không hợp lệ: '$sel'." }
    }
    Warn "$($c.Port) là $($c.Name), không phải cổng Type-C của ESP-Mosaico (Espressif 303A)."
    Warn "Chỉ đúng nếu anh đang nối dây USB-UART vào pad TX/RX/GND (TX->RX, RX->TX, GND->GND)."
    return $c
}

# ---------- Gọi esptool, trả về (thành công, nội dung log) ----------
function Invoke-Esptool([string]$p, [string]$before, [string[]]$cmd) {
    $prev = $ErrorActionPreference; $ErrorActionPreference = "Continue"
    $out = & $VenvPy -m esptool --port $p --baud 115200 --before $before --after no-reset @cmd 2>&1 | Out-String
    $code = $LASTEXITCODE; $ErrorActionPreference = $prev
    Write-Host $out
    return @(($code -eq 0), $out)
}

function Wait-BootMode([string]$why) {
    Warn $why
    Write-Host "   1. Tắt máy (giữ POWER đến khi tắt)."
    Write-Host "   2. Giữ nút BOOT, nhấn POWER, rồi thả BOOT. Màn hình tối là đúng."
    Write-Host "   3. Đóng Arduino IDE / Serial Monitor / tab esptool-js nếu đang mở."
    Read-Host "Làm xong nhấn Enter"
}

# ---------- 1. Kết nối + xác định chip ----------
$conn = $null; $before = $null
for ($try = 1; $try -le 3 -and -not $conn; $try++) {
    $c = Select-Port
    if (-not $c) {
        Wait-BootMode "Lần $($try): chưa thấy cổng nào của máy (cáp USB-C phải có data)."
        continue
    }
    $modes = if ($c.Vid -eq '303A') { @('no-reset') } else { @('default-reset', 'no-reset') }
    foreach ($m in $modes) {
        Ok "Lần $($try): hỏi chip trên $($c.Port) (reset: $m)"
        $r = Invoke-Esptool $c.Port $m @('chip-id')
        if ($r[0]) { $conn = $c; $before = $m; $chipLog = $r[1]; break }
    }
    if (-not $conn) {
        if ($r[1] -match 'Invalid head of packet|No serial data received|Failed to connect') {
            Wait-BootMode "Chip đang chạy chương trình, chưa ở chế độ nạp."
        } elseif ($r[1] -match 'could not open|PermissionError|busy|not functioning') {
            Wait-BootMode "Cổng bị chiếm hoặc lỗi driver. Rút cáp, cắm lại."
        } else {
            Wait-BootMode "Không kết nối được."
        }
    }
}
if (-not $conn) {
    Fail "Không kết nối được sau 3 lần. Đổi cáp USB-C có data, cắm thẳng cổng USB máy tính, xem Device Manager -> Ports."
}

$chip = if ($chipLog -match 'Chip type:\s*([^\r\n(]+)') { $Matches[1].Trim() }
        elseif ($chipLog -match 'Detecting chip type\.*\s*([^\r\n]+)') { $Matches[1].Trim() } else { "?" }
Ok "Chip: $chip"
if ($chip -notmatch 'S31') {
    Fail "Thiết bị trên $($conn.Port) là '$chip', KHÔNG phải ESP-Mosaico (ESP32-S31). Không nạp để tránh hỏng. Rút thiết bị đó ra, cắm ESP-Mosaico bằng cáp USB-C rồi chạy lại."
}

# ---------- 2. Nạp ----------
Ok "Nạp $File vào $($conn.Port)"
$r = Invoke-Esptool $conn.Port $before @('--chip', 'esp32s31', 'write-flash', '0x0', $File)
if (-not $r[0]) {
    Wait-BootMode "Nạp bị ngắt giữa chừng, thử lại một lần."
    $r = Invoke-Esptool $conn.Port 'no-reset' @('--chip', 'esp32s31', 'write-flash', '0x0', $File)
    if (-not $r[0]) { Fail "Nạp lỗi. Gửi đoạn log ở trên cho Claude." }
}
$Port = $conn.Port
Ok "Nạp xong. Bấm POWER để khởi động lại máy."

function Find-Port { $c = Select-Port; if ($c) { $c.Port } }

if ($Flash -eq "hwtest") {
    Write-Host "Mở log self-test (Ctrl+] để thoát). Kết quả: 1 rung dài = PASS, 3 rung ngắn = lỗi."
    Read-Host "Bấm POWER cho máy chạy, rồi nhấn Enter ở đây để mở log"
    $logPort = Find-Port
    if ($logPort) { & $VenvPy -m serial.tools.miniterm $logPort 115200 } else { Warn "Không thấy cổng log; xem kết quả bằng motor rung." }
} else {
    Write-Host "Máy phát Wi-Fi 'esp-claw-xxxx': kết nối vào, trang cài đặt tự mở (hoặc http://192.168.4.1)."
}
