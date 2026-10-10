#pragma once
// Chooses the CA certificate file for TLS verification (research R-6, constitution Principle III):
// the device's CA store when it is usable, otherwise the CA file bundled with the application.

#include <string>
#include <vector>

namespace pbnotes {

enum class CaSource { Device, Bundled };

struct CaSelection {
    std::string path;  // never empty
    CaSource source = CaSource::Bundled;
};

// A usable CA file is a readable, non-empty regular file containing a PEM certificate marker.
bool is_usable_ca_file(const std::string& path);

// First usable device candidate (source Device), else `bundled_path` (source Bundled). There is
// no "no CA" result and no way to disable verification.
CaSelection select_ca_file(const std::vector<std::string>& device_candidates, const std::string& bundled_path);

enum class CustomCaStatus { None, Loaded, Invalid };

struct EffectiveCa {
    std::string path;  // the CA file to hand to the transport (never empty)
    CustomCaStatus custom_status = CustomCaStatus::None;
    int custom_cert_count = 0;  // certificates contributed by the custom file (0 unless Loaded)
};

// Combines the base CA file (device store if usable, else bundled) with a user-supplied
// `custom-ca.pem`. A missing custom file yields the base unchanged (status None). A custom file
// that is empty, unreadable, larger than 1 MB or not made of well-formed PEM certificates is
// ignored with status Invalid and the base is used. Otherwise the base followed by the custom
// certificates is written atomically to `out_path`. Verification is never disabled, and the
// input files are never modified.
EffectiveCa build_effective_ca(const std::string& base_ca_path, const std::string& custom_ca_path,
                               const std::string& out_path);

}  // namespace pbnotes
