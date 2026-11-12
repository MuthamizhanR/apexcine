#include "apexcine/lp_platform/DeviceQuirkDatabase.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace apexcine::lp {

namespace {
std::string toLowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string extractJsonStringField(const std::string& obj, const std::string& key, const std::string& defVal) {
    const std::string pattern = "\"" + key + "\"";
    const size_t kPos = obj.find(pattern);
    if (kPos == std::string::npos) return defVal;
    const size_t colon = obj.find(':', kPos + pattern.size());
    if (colon == std::string::npos) return defVal;
    const size_t q1 = obj.find('"', colon + 1);
    if (q1 == std::string::npos) return defVal;
    const size_t q2 = obj.find('"', q1 + 1);
    if (q2 == std::string::npos) return defVal;
    return obj.substr(q1 + 1, q2 - q1 - 1);
}

int extractJsonIntField(const std::string& obj, const std::string& key, int defVal) {
    const std::string pattern = "\"" + key + "\"";
    const size_t kPos = obj.find(pattern);
    if (kPos == std::string::npos) return defVal;
    const size_t colon = obj.find(':', kPos + pattern.size());
    if (colon == std::string::npos) return defVal;
    size_t numStart = colon + 1;
    while (numStart < obj.size() && (obj[numStart] == ' ' || obj[numStart] == '\t' || obj[numStart] == '\n')) {
        ++numStart;
    }
    if (numStart >= obj.size()) return defVal;
    try {
        return std::stoi(obj.substr(numStart));
    } catch (...) {
        return defVal;
    }
}

float extractJsonFloatField(const std::string& obj, const std::string& key, float defVal) {
    const std::string pattern = "\"" + key + "\"";
    const size_t kPos = obj.find(pattern);
    if (kPos == std::string::npos) return defVal;
    const size_t colon = obj.find(':', kPos + pattern.size());
    if (colon == std::string::npos) return defVal;
    size_t numStart = colon + 1;
    while (numStart < obj.size() && (obj[numStart] == ' ' || obj[numStart] == '\t' || obj[numStart] == '\n')) {
        ++numStart;
    }
    if (numStart >= obj.size()) return defVal;
    try {
        return std::stof(obj.substr(numStart));
    } catch (...) {
        return defVal;
    }
}

bool extractJsonBoolField(const std::string& obj, const std::string& key, bool defVal) {
    const std::string pattern = "\"" + key + "\"";
    const size_t kPos = obj.find(pattern);
    if (kPos == std::string::npos) return defVal;
    const size_t colon = obj.find(':', kPos + pattern.size());
    if (colon == std::string::npos) return defVal;
    const size_t valPos = obj.find_first_not_of(" \t\r\n", colon + 1);
    if (valPos == std::string::npos) return defVal;
    if (obj.compare(valPos, 4, "true") == 0) return true;
    if (obj.compare(valPos, 5, "false") == 0) return false;
    return defVal;
}
} // namespace

DeviceQuirkDatabase::DeviceQuirkDatabase() {
    populateBuiltInVerifiedMatrix();
}

void DeviceQuirkDatabase::populateBuiltInVerifiedMatrix() {
    profiles_.clear();

    // 1. OnePlus Nord 5 (Explicit user target: Sony LYT-600 50MP OIS + IMX355 UW, Snapdragon 8s Gen 3 / 7+ Gen 3)
    {
        DeviceProfileRecord p{};
        p.profileId = "oneplus_nord5_lyt600";
        p.manufacturer = "OnePlus";
        p.modelPattern = "Nord 5|CPH2709|CPH2707";
        p.marketingName = "OnePlus Nord 5";
        p.socName = "Snapdragon 8s Gen 3 (SM8635)";
        p.primarySensorName = "Sony LYT-600 (IMX882) 50MP 1/1.95\" OIS";
        p.storageTier = "UFS 3.1 / 4.0";
        p.cfaPattern = l0::CfaPattern::RGGB;
        p.activeBitDepth = 12;
        p.blackLevelRggb[0] = 256;
        p.blackLevelRggb[1] = 256;
        p.blackLevelRggb[2] = 256;
        p.blackLevelRggb[3] = 256;
        p.whiteLevel = 4095;
        p.rowStrideAlignmentBytes = 64;
        p.rollingShutterReadout4kMs = 11.2f;
        p.rollingShutterReadout1080pMs = 5.8f;
        p.baseNativeIso = 100;
        p.dcgJumpIso = 400; // Dual Conversion Gain switch point at ISO 400
        p.maxDynamicRangeStops = 13.7f;
        p.quirkForceCpuImageReaderMap = false;
        p.quirkRowStridePaddingOnRaw10 = true;
        p.quirkPdafRowDefectFilterRequired = true;
        p.quirkGreenSplitCorrectionRequired = true;
        p.pdafDefectRowPeriod = 16;
        p.vulkanOptimalWorkgroupSizeX = 16;
        p.vulkanOptimalWorkgroupSizeY = 16;
        p.maxSustainedStorageMBps = 420;
        p.physicalCameraIds = {"0", "2", "3"};
        p.isHardwareVerified = false;
        p.colorProfile.isColorimetricallyCalibrated = false;
        profiles_.push_back(p);
    }

    // 2. OnePlus 12 / 13 (Sony LYT-808 50MP Dual-Layer Transistor Pixel)
    {
        DeviceProfileRecord p{};
        p.profileId = "oneplus_12_13_lyt808";
        p.manufacturer = "OnePlus";
        p.modelPattern = "CPH2581|CPH2583|CPH2653|OnePlus 12|OnePlus 13";
        p.marketingName = "OnePlus 12 / 13";
        p.socName = "Snapdragon 8 Gen 3 / Elite";
        p.primarySensorName = "Sony LYT-808 50MP 1/1.4\" Dual-Layer Transistor";
        p.storageTier = "UFS 4.0";
        p.cfaPattern = l0::CfaPattern::RGGB;
        p.activeBitDepth = 12;
        p.whiteLevel = 4095;
        p.rollingShutterReadout4kMs = 9.8f;
        p.rollingShutterReadout1080pMs = 4.9f;
        p.baseNativeIso = 64;
        p.dcgJumpIso = 320;
        p.maxDynamicRangeStops = 14.3f;
        p.quirkRowStridePaddingOnRaw10 = true;
        p.quirkPdafRowDefectFilterRequired = true;
        p.quirkGreenSplitCorrectionRequired = true;
        p.maxSustainedStorageMBps = 680;
        p.physicalCameraIds = {"0", "2", "3", "4"};
        p.isHardwareVerified = false;
        p.colorProfile.isColorimetricallyCalibrated = false;
        profiles_.push_back(p);
    }

    // 3. Google Pixel 8 Pro (Tensor G3, Samsung GNK 50MP)
    {
        DeviceProfileRecord p{};
        p.profileId = "google_pixel8pro_gnk";
        p.manufacturer = "Google";
        p.modelPattern = "Pixel 8 Pro|husky";
        p.marketingName = "Google Pixel 8 Pro";
        p.socName = "Google Tensor G3";
        p.primarySensorName = "Samsung ISOCELL GNK 50MP 1/1.31\"";
        p.storageTier = "UFS 3.1";
        p.cfaPattern = l0::CfaPattern::BGGR;
        p.activeBitDepth = 10;
        p.blackLevelRggb[0] = 64;
        p.blackLevelRggb[1] = 64;
        p.blackLevelRggb[2] = 64;
        p.blackLevelRggb[3] = 64;
        p.whiteLevel = 1023;
        p.rowStrideAlignmentBytes = 64;
        p.rollingShutterReadout4kMs = 13.4f;
        p.rollingShutterReadout1080pMs = 6.8f;
        p.baseNativeIso = 50;
        p.dcgJumpIso = 380;
        p.maxDynamicRangeStops = 13.5f;
        p.quirkRowStridePaddingOnRaw10 = true;
        p.quirkPdafRowDefectFilterRequired = true;
        p.vulkanOptimalWorkgroupSizeX = 8;
        p.vulkanOptimalWorkgroupSizeY = 8;
        p.maxSustainedStorageMBps = 360;
        p.physicalCameraIds = {"0", "2", "3", "4"};
        p.isHardwareVerified = false;
        p.colorProfile.isColorimetricallyCalibrated = false;
        profiles_.push_back(p);
    }

    // 4. Google Pixel 9 Pro (Tensor G4, Samsung GNK + IMX858 UW/Tele)
    {
        DeviceProfileRecord p{};
        p.profileId = "google_pixel9pro_gnk";
        p.manufacturer = "Google";
        p.modelPattern = "Pixel 9 Pro|caiman|komodo";
        p.marketingName = "Google Pixel 9 Pro";
        p.socName = "Google Tensor G4";
        p.primarySensorName = "Samsung ISOCELL GNK 50MP + IMX858 48MP UW/Tele";
        p.storageTier = "UFS 4.0";
        p.cfaPattern = l0::CfaPattern::BGGR;
        p.activeBitDepth = 12;
        p.whiteLevel = 4095;
        p.rollingShutterReadout4kMs = 12.1f;
        p.rollingShutterReadout1080pMs = 6.1f;
        p.baseNativeIso = 50;
        p.dcgJumpIso = 380;
        p.maxDynamicRangeStops = 13.9f;
        p.quirkRowStridePaddingOnRaw10 = true;
        p.vulkanOptimalWorkgroupSizeX = 8;
        p.vulkanOptimalWorkgroupSizeY = 8;
        p.maxSustainedStorageMBps = 520;
        p.physicalCameraIds = {"0", "2", "3", "4"};
        p.isHardwareVerified = false;
        p.colorProfile.isColorimetricallyCalibrated = false;
        profiles_.push_back(p);
    }

    // 5. Samsung Galaxy S24 Ultra (Snapdragon 8 Gen 3, ISOCELL HP2 200MP)
    {
        DeviceProfileRecord p{};
        p.profileId = "samsung_s24ultra_sd_hp2";
        p.manufacturer = "samsung";
        p.modelPattern = "SM-S928|Galaxy S24 Ultra|SM-S938";
        p.marketingName = "Samsung Galaxy S24 / S25 Ultra (Snapdragon)";
        p.socName = "Snapdragon 8 Gen 3 / Elite for Galaxy";
        p.primarySensorName = "Samsung ISOCELL HP2 200MP (Tetra2pixel 12.5MP/50MP RAW)";
        p.storageTier = "UFS 4.0";
        p.cfaPattern = l0::CfaPattern::GRBG;
        p.activeBitDepth = 12;
        p.whiteLevel = 4095;
        p.rollingShutterReadout4kMs = 11.8f;
        p.rollingShutterReadout1080pMs = 5.9f;
        p.baseNativeIso = 50;
        p.dcgJumpIso = 320;
        p.maxDynamicRangeStops = 13.8f;
        p.quirkPdafRowDefectFilterRequired = true;
        p.quirkGreenSplitCorrectionRequired = true;
        p.pdafDefectRowPeriod = 8;
        p.vulkanOptimalWorkgroupSizeX = 16;
        p.vulkanOptimalWorkgroupSizeY = 16;
        p.maxSustainedStorageMBps = 650;
        p.physicalCameraIds = {"0", "2", "4", "52"};
        p.isHardwareVerified = false;
        p.colorProfile.isColorimetricallyCalibrated = false;
        profiles_.push_back(p);
    }

    // 6. Samsung Galaxy S24+ (Exynos 2400, Xclipse 940 GPU)
    {
        DeviceProfileRecord p{};
        p.profileId = "samsung_s24plus_exynos_gn3";
        p.manufacturer = "samsung";
        p.modelPattern = "SM-S926B|SM-S921B|Exynos";
        p.marketingName = "Samsung Galaxy S24 / S24+ (Exynos 2400)";
        p.socName = "Samsung Exynos 2400 (Xclipse 940 RDNA3)";
        p.primarySensorName = "Samsung ISOCELL GN3 50MP";
        p.storageTier = "UFS 4.0";
        p.cfaPattern = l0::CfaPattern::GRBG;
        p.activeBitDepth = 12;
        p.whiteLevel = 4095;
        p.rollingShutterReadout4kMs = 12.9f;
        p.baseNativeIso = 50;
        p.dcgJumpIso = 400;
        p.maxDynamicRangeStops = 13.4f;
        // Conservative CPU map fallback for Exynos Xclipse until verified on target hardware
        p.quirkForceCpuImageReaderMap = true;
        p.quirkPdafRowDefectFilterRequired = true;
        p.vulkanOptimalWorkgroupSizeX = 8;
        p.vulkanOptimalWorkgroupSizeY = 8;
        p.maxSustainedStorageMBps = 540;
        p.physicalCameraIds = {"0", "2", "4"};
        p.isHardwareVerified = false;
        p.colorProfile.isColorimetricallyCalibrated = false;
        profiles_.push_back(p);
    }

    // 7. Xiaomi 14 Ultra (1-Inch Sony LYT-900, 14-bit ADC)
    {
        DeviceProfileRecord p{};
        p.profileId = "xiaomi_14ultra_lyt900";
        p.manufacturer = "Xiaomi";
        p.modelPattern = "24030PN60G|24031PN0DC|Xiaomi 14 Ultra|Xiaomi 13 Ultra";
        p.marketingName = "Xiaomi 14 Ultra (1-Inch LYT-900)";
        p.socName = "Snapdragon 8 Gen 3";
        p.primarySensorName = "Sony LYT-900 50MP 1.0-Inch Native 14-Bit CMOS";
        p.storageTier = "UFS 4.0";
        p.cfaPattern = l0::CfaPattern::RGGB;
        p.activeBitDepth = 14;
        p.blackLevelRggb[0] = 1024;
        p.blackLevelRggb[1] = 1024;
        p.blackLevelRggb[2] = 1024;
        p.blackLevelRggb[3] = 1024;
        p.whiteLevel = 16383;
        p.rollingShutterReadout4kMs = 10.4f;
        p.rollingShutterReadout1080pMs = 5.1f;
        p.baseNativeIso = 50;
        p.dcgJumpIso = 320;
        p.maxDynamicRangeStops = 14.8f;
        p.quirkGreenSplitCorrectionRequired = true;
        p.maxSustainedStorageMBps = 700;
        p.physicalCameraIds = {"0", "2", "3", "4", "5"};
        p.isHardwareVerified = false;
        p.colorProfile.isColorimetricallyCalibrated = false;
        profiles_.push_back(p);
    }

    // 8. Sony Xperia 1 VI (Exmor T IMX888 Stacked Sensor)
    {
        DeviceProfileRecord p{};
        p.profileId = "sony_xperia1vi_imx888";
        p.manufacturer = "Sony";
        p.modelPattern = "XQ-EC54|XQ-EC72|XQ-DQ54|Xperia 1 V|Xperia 1 VI";
        p.marketingName = "Sony Xperia 1 VI (Exmor T)";
        p.socName = "Snapdragon 8 Gen 3";
        p.primarySensorName = "Sony Exmor T IMX888 52MP Stacked 2-Layer Transistor";
        p.storageTier = "UFS 4.0";
        p.cfaPattern = l0::CfaPattern::RGGB;
        p.activeBitDepth = 12;
        p.whiteLevel = 4095;
        p.rollingShutterReadout4kMs = 9.2f;
        p.rollingShutterReadout1080pMs = 4.6f;
        p.baseNativeIso = 64;
        p.dcgJumpIso = 250;
        p.maxDynamicRangeStops = 14.4f;
        p.maxSustainedStorageMBps = 580;
        p.physicalCameraIds = {"0", "2", "3"};
        p.isHardwareVerified = false;
        p.colorProfile.isColorimetricallyCalibrated = false;
        profiles_.push_back(p);
    }

    // 9. Nothing Phone (2) / (2a) (Mid-high UFS 3.1 / 2.2 bandwidth target)
    {
        DeviceProfileRecord p{};
        p.profileId = "nothing_phone2_imx890";
        p.manufacturer = "Nothing";
        p.modelPattern = "A065|A142|Nothing Phone";
        p.marketingName = "Nothing Phone (2) / (2a)";
        p.socName = "Snapdragon 8+ Gen 1 / Dimensity 7200 Pro";
        p.primarySensorName = "Sony IMX890 50MP 1/1.56\" OIS";
        p.storageTier = "UFS 3.1";
        p.cfaPattern = l0::CfaPattern::RGGB;
        p.activeBitDepth = 12;
        p.whiteLevel = 4095;
        p.rollingShutterReadout4kMs = 11.6f;
        p.baseNativeIso = 100;
        p.dcgJumpIso = 400;
        p.maxDynamicRangeStops = 13.5f;
        p.maxSustainedStorageMBps = 340;
        p.physicalCameraIds = {"0", "2"};
        p.isHardwareVerified = false;
        p.colorProfile.isColorimetricallyCalibrated = false;
        profiles_.push_back(p);
    }

    // 10. Motorola Edge 50 Pro (Mid-range Snapdragon 7 Gen 3, UFS 2.2 thermal & bandwidth stress target)
    {
        DeviceProfileRecord p{};
        p.profileId = "motorola_edge50pro_sd7g3";
        p.manufacturer = "motorola";
        p.modelPattern = "XT2403|edge 50 pro|moto";
        p.marketingName = "Motorola Edge 50 Pro (SD 7 Gen 3 UFS 2.2)";
        p.socName = "Snapdragon 7 Gen 3";
        p.primarySensorName = "OmniVision OV50E 50MP 1/1.55\"";
        p.storageTier = "UFS 2.2";
        p.cfaPattern = l0::CfaPattern::BGGR;
        p.activeBitDepth = 10;
        p.blackLevelRggb[0] = 64;
        p.blackLevelRggb[1] = 64;
        p.blackLevelRggb[2] = 64;
        p.blackLevelRggb[3] = 64;
        p.whiteLevel = 1023;
        p.rollingShutterReadout4kMs = 14.2f;
        p.baseNativeIso = 100;
        p.dcgJumpIso = 400;
        p.maxDynamicRangeStops = 13.1f;
        p.quirkDisableODirectOnF2fs = true;
        p.maxSustainedStorageMBps = 190; // UFS 2.2 write cap requires lossless compression >= 2.5:1 for 4K24
        p.physicalCameraIds = {"0", "2", "3"};
        p.isHardwareVerified = false;
        p.colorProfile.isColorimetricallyCalibrated = false;
        profiles_.push_back(p);
    }
}

size_t DeviceQuirkDatabase::loadProfilesFromJson(const std::string& jsonContent) {
    size_t loadedCount = 0;
    size_t pos = 0;
    while (true) {
        const size_t idKey = jsonContent.find("\"profileId\"", pos);
        if (idKey == std::string::npos) break;
        const size_t objStart = jsonContent.rfind('{', idKey);
        const size_t nextIdKey = jsonContent.find("\"profileId\"", idKey + 11);
        const size_t objEnd = (nextIdKey != std::string::npos)
                                  ? jsonContent.rfind('}', nextIdKey)
                                  : jsonContent.rfind('}');
        if (objStart == std::string::npos || objEnd == std::string::npos || objEnd <= objStart) {
            break;
        }
        const std::string chunk = jsonContent.substr(objStart, objEnd - objStart + 1);
        DeviceProfileRecord rec{};
        rec.profileId = extractJsonStringField(chunk, "profileId", "custom_profile");
        rec.manufacturer = extractJsonStringField(chunk, "manufacturer", "Generic");
        rec.modelPattern = extractJsonStringField(chunk, "modelPattern", "*");
        rec.marketingName = extractJsonStringField(chunk, "marketingName", rec.profileId);
        rec.socName = extractJsonStringField(chunk, "socName", "ARMv8-A");
        rec.primarySensorName = extractJsonStringField(chunk, "primarySensorName", "CMOS");
        rec.storageTier = extractJsonStringField(chunk, "storageTier", "UFS 3.1");
        rec.isHardwareVerified = extractJsonBoolField(chunk, "verified", false);
        rec.activeBitDepth = static_cast<uint32_t>(extractJsonIntField(chunk, "activeBitDepth", 12));
        rec.whiteLevel = static_cast<uint16_t>(extractJsonIntField(chunk, "whiteLevel", 4095));
        rec.rollingShutterReadout4kMs = extractJsonFloatField(chunk, "rollingShutterReadout4kMs", 11.5f);
        rec.baseNativeIso = static_cast<uint32_t>(extractJsonIntField(chunk, "baseNativeIso", 100));
        rec.dcgJumpIso = static_cast<uint32_t>(extractJsonIntField(chunk, "dcgJumpIso", 400));
        rec.maxDynamicRangeStops = extractJsonFloatField(chunk, "maxDynamicRangeStops", 13.6f);
        rec.quirkForceCpuImageReaderMap = extractJsonBoolField(chunk, "quirkForceCpuImageReaderMap", false);
        rec.quirkRowStridePaddingOnRaw10 = extractJsonBoolField(chunk, "quirkRowStridePaddingOnRaw10", false);
        rec.quirkPdafRowDefectFilterRequired =
            extractJsonBoolField(chunk, "quirkPdafRowDefectFilterRequired", false);
        rec.maxSustainedStorageMBps =
            static_cast<uint32_t>(extractJsonIntField(chunk, "maxSustainedStorageMBps", 380));

        // Update existing or append
        auto it = std::find_if(profiles_.begin(), profiles_.end(), [&](const DeviceProfileRecord& existing) {
            return existing.profileId == rec.profileId;
        });
        if (it != profiles_.end()) {
            *it = rec;
        } else {
            profiles_.push_back(rec);
        }
        ++loadedCount;
        pos = objEnd + 1;
    }
    return loadedCount;
}

std::string DeviceQuirkDatabase::exportAllProfilesToJson() const {
    std::ostringstream oss;
    oss << "{\n  \"schemaVersion\": 1,\n  \"profiles\": [\n";
    for (size_t i = 0; i < profiles_.size(); ++i) {
        const auto& p = profiles_[i];
        oss << "    {\n"
            << "      \"profileId\": \"" << p.profileId << "\",\n"
            << "      \"verified\": " << (p.isHardwareVerified ? "true" : "false") << ",\n"
            << "      \"manufacturer\": \"" << p.manufacturer << "\",\n"
            << "      \"modelPattern\": \"" << p.modelPattern << "\",\n"
            << "      \"marketingName\": \"" << p.marketingName << "\",\n"
            << "      \"socName\": \"" << p.socName << "\",\n"
            << "      \"primarySensorName\": \"" << p.primarySensorName << "\",\n"
            << "      \"storageTier\": \"" << p.storageTier << "\",\n"
            << "      \"activeBitDepth\": " << p.activeBitDepth << ",\n"
            << "      \"whiteLevel\": " << p.whiteLevel << ",\n"
            << "      \"rollingShutterReadout4kMs\": " << p.rollingShutterReadout4kMs << ",\n"
            << "      \"baseNativeIso\": " << p.baseNativeIso << ",\n"
            << "      \"dcgJumpIso\": " << p.dcgJumpIso << ",\n"
            << "      \"maxDynamicRangeStops\": " << p.maxDynamicRangeStops << ",\n"
            << "      \"quirkForceCpuImageReaderMap\": "
            << (p.quirkForceCpuImageReaderMap ? "true" : "false") << ",\n"
            << "      \"quirkRowStridePaddingOnRaw10\": "
            << (p.quirkRowStridePaddingOnRaw10 ? "true" : "false") << ",\n"
            << "      \"quirkPdafRowDefectFilterRequired\": "
            << (p.quirkPdafRowDefectFilterRequired ? "true" : "false") << ",\n"
            << "      \"maxSustainedStorageMBps\": " << p.maxSustainedStorageMBps << ",\n"
            << "      \"colorimetricallyCalibrated\": "
            << (p.colorProfile.isColorimetricallyCalibrated ? "true" : "false") << "\n"
            << "    }" << (i + 1 < profiles_.size() ? "," : "") << "\n";
    }
    oss << "  ]\n}";
    return oss.str();
}

DeviceProfileRecord DeviceQuirkDatabase::matchDevice(const std::string& manufacturer,
                                                     const std::string& model) const {
    const std::string lowerMfg = toLowerAscii(manufacturer);
    const std::string lowerModel = toLowerAscii(model);

    // 1. Check pipe-delimited modelPattern tokens first (exact model match)
    for (const auto& prof : profiles_) {
        std::stringstream ss(prof.modelPattern);
        std::string token;
        while (std::getline(ss, token, '|')) {
            const std::string lowerToken = toLowerAscii(token);
            if (!lowerToken.empty() && lowerToken != "*" &&
                lowerModel.find(lowerToken) != std::string::npos) {
                return prof;
            }
        }
    }

    // 2. Manufacturer-only match: inherit ONLY OEM HAL quirks, never sensor-specific parameters
    for (const auto& prof : profiles_) {
        const std::string profMfg = toLowerAscii(prof.manufacturer);
        if (!profMfg.empty() && lowerMfg.find(profMfg) != std::string::npos) {
            DeviceProfileRecord mfgQuirksOnly{};
            mfgQuirksOnly.profileId = profMfg + "_generic_quirks_fallback";
            mfgQuirksOnly.manufacturer = prof.manufacturer;
            mfgQuirksOnly.marketingName =
                model.empty() ? (prof.manufacturer + " Generic RAW Device") : model;
            mfgQuirksOnly.isHardwareVerified = false;
            mfgQuirksOnly.quirkForceCpuImageReaderMap = prof.quirkForceCpuImageReaderMap;
            mfgQuirksOnly.quirkRowStridePaddingOnRaw10 = true;
            mfgQuirksOnly.quirkPdafRowDefectFilterRequired = prof.quirkPdafRowDefectFilterRequired;
            mfgQuirksOnly.quirkGreenSplitCorrectionRequired = true;
            mfgQuirksOnly.quirkDisableODirectOnF2fs = prof.quirkDisableODirectOnF2fs;
            return mfgQuirksOnly;
        }
    }

    // 3. Safe synthesized fallback profile
    DeviceProfileRecord fallback{};
    fallback.profileId = "safe_fallback_synthesized";
    fallback.manufacturer = manufacturer.empty() ? "Generic" : manufacturer;
    fallback.marketingName = model.empty() ? "Android RAW Camera (Safe Fallback)" : model;
    fallback.isHardwareVerified = false;
    fallback.quirkForceCpuImageReaderMap = true; // Safe CPU fallback by default on unknown HALs
    fallback.quirkRowStridePaddingOnRaw10 = true;
    fallback.quirkGreenSplitCorrectionRequired = true;
    return fallback;
}

} // namespace apexcine::lp
