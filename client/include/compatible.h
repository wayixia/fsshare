
#include <memory>

#if __cplusplus < 201402L
namespace std {
  template<typename T, typename... Args>
  unique_ptr<T> make_unique(Args&&... args) {
    return unique_ptr<T>(new T(std::forward<Args>(args)...));
  }
}
#endif