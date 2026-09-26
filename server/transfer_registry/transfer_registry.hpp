#pragma once
#include <cstdint>
#include <string>
#include <chrono>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <optional>
#include <vector>
#include <cstdint>
#include <atomic>

// an enums that represents the status of a file transfer, it can be Pending, Active, Done or Error
enum class TransferStatus {
    Pending,
    Active,
    Done,
    Error
};
// for each client transfer we will store the transfer state
struct TransferState {
    uint64_t id;
    std::string filename;
    uint64_t totalBytes;
    uint64_t bytesSent;
    TransferStatus status;
    std::chrono::steady_clock::time_point startTime;
};
// needed for formatting the transfer state into a string for sending it to the client
inline std::string statusToString(TransferStatus status) {
    switch (status) {
        case TransferStatus::Pending: return "Pending";
        case TransferStatus::Active:  return "Active";
        case TransferStatus::Done:    return "Done";
        case TransferStatus::Error:   return "Error";
    }
    return "Unknown";
}

class TransferRegistry {
public:
    uint64_t create(const std::string& filename, uint64_t totalBytes); // this function will create a new transfer state and return its unique id
    void updateProgress(uint64_t id, uint64_t bytesSent); // for each chunk of data sent we will update the progress of the transfer state
    void setStatus(uint64_t id, TransferStatus status); // we use this function in two caases, when the transfer is done or when an error occurs during the transfer
    std::optional<TransferState> get(uint64_t id) const; // this function will return the transfer state for a given id, if it exists
    std::vector<TransferState> getAll() const; // this function will return all transfer states
    void remove(uint64_t id); // this function will remove a transfer state from the registry typically when the transfer is done or when an error occurs

private:
    mutable std::shared_mutex mutex_; // used a shared mutex to allow multiple readers but only one writer at a time
    std::unordered_map<uint64_t, TransferState> transfers_; // we store each transfer state in an unordered map with the transfer id as the key for fast access
    std::atomic<uint64_t> nextId_{1}; // atomic counter to generate unique transfer ids, we start from 1 and increment for each new transfer
};
struct Stats {
    std::atomic<uint64_t> totalBytesTransferred{0};
    std::atomic<int> activeTransferCount{0};
};