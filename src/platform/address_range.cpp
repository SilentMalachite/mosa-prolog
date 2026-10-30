// アドレス空間の予約と確保（ADR 0010）。OS ごとの分岐はここだけに書く。
// POSIX は mmap(PROT_NONE) で予約して mprotect で確保し、Windows は VirtualAlloc の RESERVE と COMMIT を使う。
#include "mosa/address_range.hpp"

#include <utility>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace mosa {

namespace {

// ページの大きさは変わらないので一度だけ問い合わせる。
std::size_t page_size() {
  static const std::size_t size = [] {
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return static_cast<std::size_t>(info.dwPageSize);
#else
    return static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
  }();
  return size;
}

// 切り上げで桁があふれたら 0 を返す。
std::size_t round_up_pages(std::size_t bytes) {
  const std::size_t page = page_size();
  const std::size_t rem = bytes % page;
  if (rem == 0) return bytes;
  const std::size_t rounded = bytes + (page - rem);
  return rounded < bytes ? 0 : rounded;
}

}

std::optional<AddressRange> AddressRange::reserve(std::size_t bytes) {
  const std::size_t size = round_up_pages(bytes == 0 ? 1 : bytes);
  if (size == 0) return std::nullopt;
#if defined(_WIN32)
  void* base = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);
  if (base == nullptr) return std::nullopt;
#else
  // MAP_NORESERVE は、大きな予約がオーバーコミット計上で断られないようにするため。
  void* base = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (base == MAP_FAILED) return std::nullopt;
#endif
  return AddressRange(base, size);
}

AddressRange::AddressRange(AddressRange&& other) noexcept
    : base_(std::exchange(other.base_, nullptr)),
      reserved_(std::exchange(other.reserved_, 0)),
      committed_(std::exchange(other.committed_, 0)) {}

AddressRange& AddressRange::operator=(AddressRange&& other) noexcept {
  if (this != &other) {
    release();
    base_ = std::exchange(other.base_, nullptr);
    reserved_ = std::exchange(other.reserved_, 0);
    committed_ = std::exchange(other.committed_, 0);
  }
  return *this;
}

AddressRange::~AddressRange() { release(); }

void AddressRange::release() {
  if (base_ == nullptr) return;
#if defined(_WIN32)
  VirtualFree(base_, 0, MEM_RELEASE);
#else
  munmap(base_, reserved_);
#endif
  base_ = nullptr;
  reserved_ = 0;
  committed_ = 0;
}

bool AddressRange::commit(std::size_t bytes) {
  if (bytes <= committed_) return true;
  const std::size_t target = round_up_pages(bytes);
  if (target == 0 || target > reserved_) return false;
  // すでに確保した先頭側には触れず、増える分だけを確保する。
  const std::size_t extra = target - committed_;
  auto* start = static_cast<unsigned char*>(base_) + committed_;
#if defined(_WIN32)
  if (VirtualAlloc(start, extra, MEM_COMMIT, PAGE_READWRITE) == nullptr) return false;
#else
  if (mprotect(start, extra, PROT_READ | PROT_WRITE) != 0) return false;
#endif
  committed_ = target;
  return true;
}

std::uint64_t* AddressRange::words() const { return static_cast<std::uint64_t*>(base_); }
std::size_t AddressRange::reserved_bytes() const { return reserved_; }
std::size_t AddressRange::committed_bytes() const { return committed_; }

}
