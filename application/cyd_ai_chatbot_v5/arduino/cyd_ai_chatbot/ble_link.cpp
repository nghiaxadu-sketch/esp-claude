#include "ble_link.h"

#include <NimBLEDevice.h>

namespace ble_link {
namespace {

constexpr const char *kDeviceName = "CYD-Claude";
constexpr size_t kMaxQueued = 48;
constexpr size_t kRxBufferBytes = 1024;
constexpr uint16_t kScanInterval = 100;  // units of 0.625 ms
constexpr uint16_t kScanWindow = 90;
constexpr uint32_t kConnectTimeoutMs = 6000;
constexpr size_t kMinFreeHeapForBle = 60 * 1024;
constexpr size_t kMaxWriteChunk = 20;  // default ATT MTU payload

bool initialized = false;
bool scanActive = false;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;  // receive ring buffer (no allocation inside)
SemaphoreHandle_t queueLock = nullptr;            // advert queue (allocates, so a mutex, not a spinlock)
std::vector<Advert> queue;

void lockQueue()
{
    if (queueLock == nullptr) {
        queueLock = xSemaphoreCreateMutex();
    }
    xSemaphoreTake(queueLock, portMAX_DELAY);
}

void unlockQueue()
{
    xSemaphoreGive(queueLock);
}
NimBLEClient *client = nullptr;
NimBLERemoteCharacteristic *rxChar = nullptr;
NimBLERemoteCharacteristic *txChar = nullptr;
char rxBuffer[kRxBufferBytes];
size_t rxHead = 0;
size_t rxCount = 0;

class ScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice *dev) override
    {
        Advert a;
        a.name = dev->haveName() ? String(dev->getName().c_str()) : String();
        a.address = String(dev->getAddress().toString().c_str());
        a.addressType = dev->getAddress().getType();
        a.rssi = dev->getRSSI();
        if (dev->haveManufacturerData()) {
            a.manufacturerData = dev->getManufacturerData();
        }
        lockQueue();
        if (queue.size() >= kMaxQueued) {
            queue.erase(queue.begin());
        }
        queue.push_back(std::move(a));
        unlockQueue();
    }

    void onScanEnd(const NimBLEScanResults &, int) override { scanActive = false; }
};

ScanCallbacks scanCallbacks;

void onNotify(NimBLERemoteCharacteristic *, uint8_t *data, size_t len, bool)
{
    portENTER_CRITICAL(&mux);
    for (size_t i = 0; i < len; ++i) {
        if (rxCount == kRxBufferBytes) {
            rxHead = (rxHead + 1) % kRxBufferBytes;  // drop the oldest byte
            --rxCount;
        }
        rxBuffer[(rxHead + rxCount) % kRxBufferBytes] = (char)data[i];
        ++rxCount;
    }
    portEXIT_CRITICAL(&mux);
}

}  // namespace

bool begin(String &error)
{
    if (initialized) {
        return true;
    }
    if (ESP.getFreeHeap() < kMinFreeHeapForBle) {
        error = "Thieu bo nho cho Bluetooth";
        return false;
    }
    if (!NimBLEDevice::init(kDeviceName)) {
        error = "Khong bat duoc Bluetooth";
        return false;
    }
    initialized = true;
    return true;
}

void end()
{
    if (!initialized) {
        return;
    }
    stopScan();
    disconnect();
    NimBLEDevice::deinit(true);
    initialized = false;
    lockQueue();
    queue.clear();
    queue.shrink_to_fit();
    unlockQueue();
}

bool ready()
{
    return initialized;
}

bool startScan(uint32_t durationMs, bool active, bool duplicates)
{
    if (!initialized) {
        return false;
    }
    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&scanCallbacks, duplicates);
    scan->setActiveScan(active);
    scan->setInterval(kScanInterval);
    scan->setWindow(kScanWindow);
    scan->setMaxResults(0);  // results only go to the callback, nothing is kept in RAM
    scanActive = scan->start(durationMs, false, true);
    return scanActive;
}

void stopScan()
{
    if (initialized && scanActive) {
        NimBLEDevice::getScan()->stop();
    }
    scanActive = false;
}

bool scanning()
{
    return scanActive;
}

size_t takeAdverts(std::vector<Advert> &out)
{
    std::vector<Advert> taken;
    lockQueue();
    taken.swap(queue);
    unlockQueue();
    for (Advert &a : taken) {
        out.push_back(std::move(a));
    }
    return taken.size();
}

bool connectSerial(const String &address, uint8_t addressType, String &error)
{
    if (!initialized) {
        error = "Bluetooth chua bat";
        return false;
    }
    stopScan();
    disconnect();
    client = NimBLEDevice::createClient();
    if (client == nullptr) {
        error = "Thieu bo nho cho ket noi BLE";
        return false;
    }
    client->setConnectTimeout(kConnectTimeoutMs);
    const NimBLEAddress addr(std::string(address.c_str()), addressType);
    if (!client->connect(addr)) {
        error = "Thiet bi khong bat may";
        disconnect();
        return false;
    }
    // Pick a notify characteristic and a write characteristic, preferring both in the same service.
    for (NimBLERemoteService *svc : client->getServices(true)) {
        NimBLERemoteCharacteristic *notify = nullptr;
        NimBLERemoteCharacteristic *write = nullptr;
        for (NimBLERemoteCharacteristic *ch : svc->getCharacteristics(true)) {
            if (notify == nullptr && (ch->canNotify() || ch->canIndicate())) {
                notify = ch;
            }
            if (write == nullptr && (ch->canWrite() || ch->canWriteNoResponse())) {
                write = ch;
            }
        }
        if (notify != nullptr && write != nullptr) {
            rxChar = notify;
            txChar = write;
            break;
        }
    }
    if (rxChar == nullptr || txChar == nullptr) {
        error = "Khong tim thay dac tinh doc/ghi";
        disconnect();
        return false;
    }
    if (!rxChar->subscribe(rxChar->canNotify(), onNotify)) {
        error = "Khong dang ky nhan du lieu duoc";
        disconnect();
        return false;
    }
    portENTER_CRITICAL(&mux);
    rxHead = rxCount = 0;
    portEXIT_CRITICAL(&mux);
    return true;
}

void disconnect()
{
    rxChar = nullptr;
    txChar = nullptr;
    if (client != nullptr) {
        if (client->isConnected()) {
            client->disconnect();
        }
        NimBLEDevice::deleteClient(client);
        client = nullptr;
    }
}

bool serialConnected()
{
    return client != nullptr && client->isConnected() && txChar != nullptr;
}

bool writeBytes(const uint8_t *data, size_t len)
{
    if (!serialConnected()) {
        return false;
    }
    const bool withResponse = !txChar->canWriteNoResponse();
    for (size_t off = 0; off < len; off += kMaxWriteChunk) {
        const size_t n = len - off < kMaxWriteChunk ? len - off : kMaxWriteChunk;
        if (!txChar->writeValue(data + off, n, withResponse)) {
            return false;
        }
    }
    return true;
}

size_t readBytes(char *out, size_t max)
{
    portENTER_CRITICAL(&mux);
    size_t n = 0;
    while (n < max && rxCount > 0) {
        out[n++] = rxBuffer[rxHead];
        rxHead = (rxHead + 1) % kRxBufferBytes;
        --rxCount;
    }
    portEXIT_CRITICAL(&mux);
    return n;
}

}  // namespace ble_link
