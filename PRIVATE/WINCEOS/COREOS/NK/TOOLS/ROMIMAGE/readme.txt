Romimage project notes

------------------------------------------------------------------------------
!WARNINGS!

The following are general code warnings due to oddities of the whole works

1) "==" and "!=" are overloaded for string and char to be case insensitive
2) "find" and "rfind" are not!  beware!

------------------------------------------------------------------------------
Hierarchy:
  \romimage - main romimage tool

  \catbin   - a utility to concatenate records in a bin file and multiple bin 
              files together
  \compress - a utility to compress bin records using the cecompression 
              algorithms
  \sortbin  - a utility to just sort the records in a bin file by rom address
  \compbin  - concatenation and compression library
  \data     - library for binary data manipulation
  \viewbin  - utility to view the contents of a bin file
  \cvrtbin  - utility to convert a bin file to nb0 or sre format (really a 
              conditionally compiled version of viewbin code)
  \stampbin - stamps a signature into the image

  \checksymbols - checks the symbols?
  

------------------------------------------------------------------------------
Compilation:
  Romimage is written to be compiled from a command like build window created 
  with "%_WINCEROOT%\public\common\oak\misc\wince.bat NTANSI iabase CEPC"
  
  During the linking phase there will be several errors about crypto PDB files 
  missing.  This is expected and perfectly fine.  Something about the crypto 
  people not wanting the rest of us to be able to debug the stuff, no fun :)

  The ribld.bat file is provided and will compile all three verions (regular, 
  180 day timebomb, and 120 day timebomb), check out the apropriate files and 
  copy the new ones there.  There will be all sorts of cryptic output of this 
  process that I am sorry for but shouldn't be too hard to parse.  Warning as 
  above, YOU WILL SEE SEVERAL ERROR MESSAGES THAT ARE NOT ERRORS!


------------------------------------------------------------------------------
Formats:
  struct BinFile{
    BYTE signature[7];     // = { 'B', '0', '0', '0', 'F', 'F', '\a' }
    DWORD   ImageStart
    DWORD   ImageLength
    Record  ImageRecords[ImageLength]
  };

  struct Record{
    DWORD address;
    DWORD length;
    DWORD chksum;
  };

  Image

  Offset   -----------
  0x00    | ImageStart    
          |
          |
  0x40    | Rom Signature { 0x43454345 }
  0x44    | Pointer to the ROMHDR for this Region
          |
          |
  0x????  | ROMHDR{
          | };
          |


  Chain file

  struct _XIPCHAIN_ENTRY {
    LPVOID  pvAddr;                 // address of the XIP
    DWORD   dwLength;               // the size of the XIP
    DWORD   dwMaxLength;            // the biggest it can grow to
    USHORT  usOrder;                // where to put into ROMChain_t
    USHORT  usFlags;                // flags/status of XIP
    DWORD   dwVersion;              // version info
    CHAR    szName[XIP_NAMELEN];    // Name of XIP, typically the bin file's name, w/o .bin
    DWORD   dwAlgoFlags;            // algorithm to use for signature verification
    DWORD   dwKeyLen;               // length of key in byPublicKey
    BYTE    byPublicKey[596];       // public key data
  };

  
  Offset   -----------
  0x00    | DWORD Count;
  0x04    | XIPCHAIN_ENTRY[1]{
          | };
          | XIPCHAIN_ENTRY[2]{
          | };
          | ...

------------------------------------------------------------------------------
Extensions:
  Each ROMHDR contains a PVOID pExtentions that is the head of the extension 
  linked list.  The first extention is of type ROMPID and all others are of 
  type EXTENSION.  The list will be NULL terminated

  The name of the extension is limited to 32 characters counting a termnating 
  NULL character.

  The type should be used for differentiating extentions of similar name.

