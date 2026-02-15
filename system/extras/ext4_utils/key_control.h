#include <sys/cdefs.h>
#include <sys/types.h>

__BEGIN_DECLS

// ext4enc:TODO - get these keyring standard definitions from proper system file
// keyring serial number type
typedef int32_t key_serial_t;

// special process keyring shortcut IDs
#define KEY_SPEC_THREAD_KEYRING       -1 // key ID for thread-specific keyring
#define KEY_SPEC_PROCESS_KEYRING      -2 // key ID for process-specific keyring
#define KEY_SPEC_SESSION_KEYRING      -3 // key ID for session-specific keyring
#define KEY_SPEC_USER_KEYRING         -4 // key ID for UID-specific keyring
#define KEY_SPEC_USER_SESSION_KEYRING -5 // key ID for UID-session keyring
#define KEY_SPEC_GROUP_KEYRING        -6 // key ID for GID-specific keyring

key_serial_t add_key(const char *type,
                     const char *description,
                     const void *payload,
                     size_t plen,
                     key_serial_t ringid);

long keyctl_revoke(key_serial_t id);

long keyctl_setperm(key_serial_t id, int permissions);

long keyctl_search(key_serial_t ringid, const char *type,
                   const char *description, key_serial_t destringid);

__END_DECLS
