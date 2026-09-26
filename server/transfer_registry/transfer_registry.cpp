#include "transfer_registry.hpp"

// in all functions that require writing to the transfers_ map, we use a unique_lock to lock the shared_mutex like create , updateProgress, setStatus and remove ensuring exclusive access. For read-only operations like get and getAll, we use a shared_lock, allowing multiple threads to read concurrently
uint64_t TransferRegistry::create(const std::string& filename, uint64_t totalBytes) {
   uint64_t new_id = nextId_.fetch_add(1);
    TransferState new_transfer{new_id, filename, totalBytes, 0, TransferStatus::Active, std::chrono::steady_clock::now()}; 
    std::unique_lock<std::shared_mutex> lock(mutex_);
    transfers_[new_id] = new_transfer;
    
    return new_id;

}

void TransferRegistry::updateProgress(uint64_t id, uint64_t bytesSent) {
     std::unique_lock<std::shared_mutex> lock(mutex_);
     auto it = transfers_.find(id);
     if(it != transfers_.end()){
      it->second.bytesSent = bytesSent;
     }
    
}

void TransferRegistry::setStatus(uint64_t id, TransferStatus status) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = transfers_.find(id);
    if(it != transfers_.end()){
     it->second.status = status;
    }
}

std::optional<TransferState> TransferRegistry::get(uint64_t id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = transfers_.find(id);
    if(it != transfers_.end()) {
        return it->second;
    }
    return std::nullopt;

}

std::vector<TransferState> TransferRegistry::getAll() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<TransferState> vec;
    for(const auto& pair : transfers_) {
     vec.push_back(pair.second);
    }
    return vec;
}

void TransferRegistry::remove(uint64_t id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    transfers_.erase(id);

}
