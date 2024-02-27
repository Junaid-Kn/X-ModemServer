/*Process an entire message using CRC16 and return the CRC16.
  key: CRC16 key being used
  message: message to be crc'd.
num_bytes: size of the message in bytes
*/

unsigned short crc_message(unsigned int key, unsigned char *message, int num_bytes);
