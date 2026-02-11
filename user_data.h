#ifndef PLAITS_USER_DATA_H_
#define PLAITS_USER_DATA_H_

#include "stmlib.h"

// 1. Zdefiniuj platformę na samym początku
#ifndef PLAITS_DAISY
#define PLAITS_DAISY 1
#endif

namespace plaits {

// 2. Blok dla Daisy (zawsze aktywny, gdy PLAITS_DAISY jest zdefiniowane)
#if defined(PLAITS_DAISY)

class UserDataProvider {
 public:
  virtual const uint8_t* ptr(int slot) const = 0;
};

extern UserDataProvider* g_user_data_provider;

class UserData {
 public:
  enum { SIZE = 0x1000 };
  UserData() { }
  ~UserData() { }

  inline const uint8_t* ptr(int slot) const {
    if (g_user_data_provider) {
      return g_user_data_provider->ptr(slot);
    }
    return NULL;
  }
  
  inline bool Save(uint8_t* rx_buffer, int slot) {
    return false;
  }
};

// 3. Blok dla testów (używamy #elif, aby zachować ciągłość logiczną)
#elif defined(TEST)

class UserData {
 public:
  enum { ADDRESS = 0x08007000, SIZE = 0x1000 };
  UserData() { }
  inline const uint8_t* ptr(int slot) const { return NULL; }
  bool Save(uint8_t* rx_buffer, int slot); 
};

// 4. Blok dla oryginalnego hardware'u (zakomentowany dla bezpieczeństwa)
#else 

// Tutaj byłby kod dla STM32F37x

#endif 

}  // namespace plaits

#endif  // PLAITS_USER_DATA_H_