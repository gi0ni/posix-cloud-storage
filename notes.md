# disk size
* du -h -d 1 . | sort -h

# nvim
100gg in visual to jump to line

# database
* use xml

# algorithms
##### key exchange
* RSA      nah.
* DH - modular  need big key size for n
                3072 bits => 128 bit level security => 2^128 operations needed to crack
* ECDH      - tangents instead of modulo
              256 bits => 128 bit level security
              eq. y^2 = x^3 + ax + b
              NIST P-256 curve - CSPRNG Dual_EC_DRBG - NSA backdoor..?
              Curve25519
* ECDHE         ephemeral - solves man-in-the-middle??
##### password hashing
* bscrypt  ???
##### key derivation
* PBKDF2   ??? (password based key derivation)
* scrypt   ???
* Argon2   ???
##### encryption (AE/AEAD) ## AEAD also checks for cheksum or packet or smth..?
* DES      no. broken. also slow.
* Twofish  ???
* AES (Rjindael)  multiple rounds of: substitutions + jumble row elems and columns (w/ matrix mult) + XOR with key
                  128 bits is fine
                  THERE ARE ASM INSTRUCTIONS FOR THIS. GB/s
* AES-GCM       also does signature. IV || cyphertext || MAC
* ChaCha20 ???
##### cheksum
* MD5      no. broken. weak to collisions.
* CRC32    faster but more collisions
* Sha-2    sha256 should be good enough.
##### message signature
* RSA      modular arithmetic. euler's totient and modular inverse all that shebang.
* DSA      ???
* ECDSA    ???

# protocol
1. password hashing: server-side
2. encryption: client-side

#### Phase 1: Authentification
1. TLS handshake (RSA or ECDH)
...
2. client sends password
3. server hashes password with salt and compares to stored value (bcrypt)
4. if ok send encryption salt to client
5. client hashes received salt with password to get encryption key (argon2)
    # ? should you run the password through a hash to make sure it's random enough
    # NO. argon2 should take care of that

* server needs to store salt and hash + another salt - for each client

#### Phase 2: Encryption
* client sends encrypted files to server
* client receives encrypted files from server
* only client can encrypt and decrypt

## WARNINGS
* /dev/random can block
