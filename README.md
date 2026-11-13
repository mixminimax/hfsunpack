# HFS (Heroes File System) Unpacker Release note

## The History of Vindictus HFS
1. At the game launch date (2009), the game was barely encrypted. The archive
  was basically standard zip file.
1. Then the basic encryption is introduced. It still relies on standard zip
  format but with payloads are encrypted via XOR. Each XOR keys are based on
  metadata so it could be recovered even via brute forcing. Because last 4
  characters are always ".comp"
   * Time around this, some skiddie mocked their "encryption" and this results
   them to introduce hardening the encryption. Even there were news articles
   about this. Also well, He is not even the tool writer but just a consumer 
   and bragged about this everywhere including his own blog. Small fun thing to
   laughable at.
1. In this era, hfs keep being updated. But the fundamental format is the same
  so the tools like 'VZipFlip' gets updated as well as 'HFSExtract'.
1. 2018, HFS version 2 introduced. All existing tools became useless.
  It uses WAKE-like (WAKE with modifired AES table) cryptography.
  (But hear me out. It's always not a great to use non-standard cipher that is
  not peer reviewed algorithm. I guess they'd like to put everything under the
  shadow. Who knows?)
1. Until 2021, No tool can unpack HFS'. Then Ekey released updated version
2. 2022, HFS now encrypted with each different keys. Previously it shared one
  same key. No tools updated. But still able to unpack existing files.
  (Also by the way, this key implementation is crude. The program tries to
  decrypt file try each keys until it success.)
1. 2025, HFS update again. Now it dropped WAKE-AES and adopt "ChaCha20"
  instead.
1. Initially, this tool was personal only and was meant to release AFTER the 
   EOL of this game. But I've noticed that someone made their own so I upload
   my own.

### Fun Facts
All codes of previous algorithms are still in the binary. So technically it can
open legacy files. It is just there. But not being used. Basically junks

## Algorithm
Too long to explain. Check the source code.
But in short. File name as a key, Encrypted Index, Encrypted offset,
Encrypted payload, Only first 0x400 bytes are encrypted and rest are just 
raw zlib payload, Each files are encrypted with different key, but this key 
is also encrypted so do XOR with more, nonce and counters are encrypted for 
each files too, decryption sequence for index needs to follow specific rule
because counter for chacha20 is not increase by one for each and sometime it
resets to 0, scripts and sqlite files are encrypted more, first byte is XORed,
and perfrom this xorkey * xorbyteAtTheFirstByteOftheFile but this xor table is
encrypted too with different xor value on each session and this recovering
sequence is hidden under VMProtected stuff but I putted decrypted table anyway
(because it is faster. Why border to generate each at the runtime?).
Basically Many many XOR. XOR everywhere.