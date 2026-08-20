#pragma once

#include <variant>
#include <string>
#include <system_error>
#include <utility>
#include <functional>

// Result type for unified error handling
// Replaces bool return values with proper error information
template <typename T, typename E = std::error_code>
class Result {
public:
    using value_type = T;
    using error_type = E;
    
    // Success constructors
    Result() = delete; // Prevent default construction
    
    Result(T&& value) : data_(std::in_place_index<0>, std::move(value)) {}
    Result(const T& value) : data_(std::in_place_index<0>, value) {}
    
    // Error constructors
    Result(E&& error) : data_(std::in_place_index<1>, std::move(error)) {}
    Result(const E& error) : data_(std::in_place_index<1>, error) {}
    
    // Factory methods
    static Result Ok(T&& value) { return Result(std::move(value)); }
    static Result Ok(const T& value) { return Result(value); }
    static Result Err(E&& error) { return Result(std::move(error)); }
    static Result Err(const E& error) { return Result(error); }
    
    // Check if success
    bool is_ok() const { return data_.index() == 0; }
    bool is_err() const { return data_.index() == 1; }
    
    // Access value (asserts on error)
    T& value() & { 
        if (is_err()) throw std::bad_variant_access();
        return std::get<0>(data_); 
    }
    const T& value() const & { 
        if (is_err()) throw std::bad_variant_access();
        return std::get<0>(data_); 
    }
    T&& value() && { 
        if (is_err()) throw std::bad_variant_access();
        return std::get<0>(std::move(data_)); 
    }
    
    // Access error (asserts on success)
    E& error() & { 
        if (is_ok()) throw std::bad_variant_access();
        return std::get<1>(data_); 
    }
    const E& error() const & { 
        if (is_ok()) throw std::bad_variant_access();
        return std::get<1>(data_); 
    }
    E&& error() && { 
        if (is_ok()) throw std::bad_variant_access();
        return std::get<1>(std::move(data_)); 
    }
    
    // Safe access with default
    T value_or(T&& default_value) & { return is_ok() ? value() : std::move(default_value); }
    T value_or(const T& default_value) const & { return is_ok() ? value() : default_value; }
    
    // Monadic operations (C++23 style)
    template <typename F>
    auto and_then(F&& f) & {
        using U = std::invoke_result_t<F, T&>;
        static_assert(std::is_same_v<typename U::value_type, typename U::value_type>, "and_then must return Result");
        if (is_err()) return U::Err(std::move(error()));
        return f(value());
    }
    
    template <typename F>
    auto and_then(F&& f) && {
        using U = std::invoke_result_t<F, T&&>;
        static_assert(std::is_same_v<typename U::value_type, typename U::value_type>, "and_then must return Result");
        if (is_err()) return U::Err(std::move(error()));
        return f(std::move(value()));
    }
    
    template <typename F>
    auto or_else(F&& f) & {
        using U = std::invoke_result_t<F, E&>;
        static_assert(std::is_same_v<typename U::value_type, T>, "or_else must return Result<T>");
        if (is_ok()) return *this;
        return f(error());
    }
    
    template <typename F>
    auto or_else(F&& f) && {
        using U = std::invoke_result_t<F, E&&>;
        static_assert(std::is_same_v<typename U::value_type, T>, "or_else must return Result<T>");
        if (is_ok()) return std::move(*this);
        return f(std::move(error()));
    }
    
    template <typename F>
    auto map(F&& f) & {
        using U = std::invoke_result_t<F, T&>;
        using NewResult = Result<U, E>;
        if (is_err()) return NewResult::Err(error());
        return NewResult::Ok(f(value()));
    }
    
    template <typename F>
    auto map(F&& f) && {
        using U = std::invoke_result_t<F, T&&>;
        using NewResult = Result<U, E>;
        if (is_err()) return NewResult::Err(std::move(error()));
        return NewResult::Ok(f(std::move(value())));
    }
    
    template <typename F>
    auto map_err(F&& f) & {
        using NewError = std::invoke_result_t<F, E&>;
        using NewResult = Result<T, NewError>;
        if (is_ok()) return NewResult::Ok(value());
        return NewResult::Err(f(error()));
    }
    
    template <typename F>
    auto map_err(F&& f) && {
        using NewError = std::invoke_result_t<F, E&&>;
        using NewResult = Result<T, NewError>;
        if (is_ok()) return NewResult::Ok(std::move(value()));
        return NewResult::Err(f(std::move(error())));
    }
    
    // Boolean conversion
    explicit operator bool() const { return is_ok(); }
    
    // Equality
    bool operator==(const Result& other) const {
        if (is_ok() && other.is_ok()) return value() == other.value();
        if (is_err() && other.is_err()) return error() == other.error();
        return false;
    }

private:
    std::variant<T, E> data_;
};

// Specialization for void result
template <typename E>
class Result<void, E> {
public:
    using value_type = void;
    using error_type = E;
    
    Result() : data_(std::in_place_index<0>) {}
    Result(E&& error) : data_(std::in_place_index<1>, std::move(error)) {}
    Result(const E& error) : data_(std::in_place_index<1>, error) {}
    
    static Result Ok() { return Result(); }
    static Result Err(E&& error) { return Result(std::move(error)); }
    static Result Err(const E& error) { return Result(error); }
    
    bool is_ok() const { return data_.index() == 0; }
    bool is_err() const { return data_.index() == 1; }
    
    void value() const { 
        if (is_err()) throw std::bad_variant_access(); 
    }
    
    E& error() & { 
        if (is_ok()) throw std::bad_variant_access();
        return std::get<1>(data_); 
    }
    const E& error() const & { 
        if (is_ok()) throw std::bad_variant_access();
        return std::get<1>(data_); 
    }
    
    template <typename F>
    auto and_then(F&& f) & {
        using U = std::invoke_result_t<F>;
        static_assert(std::is_same_v<typename U::value_type, void>, "and_then must return Result<void>");
        if (is_err()) return U::Err(error());
        return f();
    }
    
    explicit operator bool() const { return is_ok(); }

private:
    std::variant<std::monostate, E> data_;
};

// Common error codes for CNCS
namespace cncs_error {
    enum class Code : int {
        Success = 0,
        
        // Generic errors
        InvalidParameter = 1000,
        NotInitialized = 1001,
        AlreadyInitialized = 1002,
        NullPointer = 1003,
        BufferTooSmall = 1004,
        OutOfMemory = 1005,
        
        // Driver errors
        DriverNotLoaded = 2000,
        DriverLoadFailed = 2001,
        DriverUnloadFailed = 2002,
        DeviceOpenFailed = 2003,
        IoctlFailed = 2004,
        AccessDenied = 2005,
        InvalidHandle = 2006,
        
        // Process errors
        ProcessNotFound = 3000,
        ProcessAttachFailed = 3001,
        ProcessDetachFailed = 3002,
        ModuleNotFound = 3003,
        OffsetDumpFailed = 3004,
        
        // Memory errors
        MemoryReadFailed = 4000,
        MemoryWriteFailed = 4001,
        AddressOutOfRange = 4002,
        InvalidAddress = 4003,
        
        // Renderer errors
        WindowCreationFailed = 5000,
        DeviceCreationFailed = 5001,
        ImGuiInitFailed = 5002,
        SwapChainFailed = 5003,
        
        // Config errors
        ConfigNotFound = 6000,
        ConfigParseFailed = 6001,
        ConfigWriteFailed = 6002,
        
        // Visibility errors
        CollisionMeshNotFound = 7000,
        CollisionMeshLoadFailed = 7001,
        RayTraceFailed = 7002,
    };
    
    inline std::error_code make_error_code(Code c) {
        return std::error_code(static_cast<int>(c), std::generic_category());
    }
    
    inline std::string to_string(Code c) {
        switch (c) {
            case Code::Success: return "Success";
            case Code::InvalidParameter: return "Invalid parameter";
            case Code::NotInitialized: return "Not initialized";
            case Code::AlreadyInitialized: return "Already initialized";
            case Code::NullPointer: return "Null pointer";
            case Code::BufferTooSmall: return "Buffer too small";
            case Code::OutOfMemory: return "Out of memory";
            case Code::DriverNotLoaded: return "Driver not loaded";
            case Code::DriverLoadFailed: return "Driver load failed";
            case Code::DriverUnloadFailed: return "Driver unload failed";
            case Code::DeviceOpenFailed: return "Device open failed";
            case Code::IoctlFailed: return "IOCTL failed";
            case Code::AccessDenied: return "Access denied";
            case Code::InvalidHandle: return "Invalid handle";
            case Code::ProcessNotFound: return "Process not found";
            case Code::ProcessAttachFailed: return "Process attach failed";
            case Code::ProcessDetachFailed: return "Process detach failed";
            case Code::ModuleNotFound: return "Module not found";
            case Code::OffsetDumpFailed: return "Offset dump failed";
            case Code::MemoryReadFailed: return "Memory read failed";
            case Code::MemoryWriteFailed: return "Memory write failed";
            case Code::AddressOutOfRange: return "Address out of range";
            case Code::InvalidAddress: return "Invalid address";
            case Code::WindowCreationFailed: return "Window creation failed";
            case Code::DeviceCreationFailed: return "Device creation failed";
            case Code::ImGuiInitFailed: return "ImGui initialization failed";
            case Code::SwapChainFailed: return "Swap chain failed";
            case Code::ConfigNotFound: return "Config not found";
            case Code::ConfigParseFailed: return "Config parse failed";
            case Code::ConfigWriteFailed: return "Config write failed";
            case Code::CollisionMeshNotFound: return "Collision mesh not found";
            case Code::CollisionMeshLoadFailed: return "Collision mesh load failed";
            case Code::RayTraceFailed: return "Ray trace failed";
            default: return "Unknown error";
        }
    }
}

// Make cncs_error::Code work with std::error_code
namespace std {
    template<>
    struct is_error_code_enum<cncs_error::Code> : true_type {};
}