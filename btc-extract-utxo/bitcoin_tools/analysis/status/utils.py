import plyvel
import sys
from binascii import hexlify, unhexlify
import ujson
import struct
from math import ceil
from copy import deepcopy
from bitcoin_tools.analysis.status import *
from bitcoin_tools.utils import change_endianness
from bitcoin_tools.core.script import OutputScript


def txout_compress(n):
    """ Compresses the Satoshi amount of a UTXO to be stored in the LevelDB. Code is a port from the Bitcoin Core C++
    source:
        https://github.com/bitcoin/bitcoin/blob/v0.13.2/src/compressor.cpp#L133#L160

    :param n: Satoshi amount to be compressed.
    :type n: int
    :return: The compressed amount of Satoshis.
    :rtype: int
    """

    if n == 0:
        return 0
    e = 0
    while ((n % 10) == 0) and e < 9:
        n /= 10
        e += 1

    if e < 9:
        d = (n % 10)
        assert (1 <= d <= 9)
        n /= 10
        return 1 + (n * 9 + d - 1) * 10 + e
    else:
        return 1 + (n - 1) * 10 + 9


def txout_decompress(x):
    """ Decompresses the Satoshi amount of a UTXO stored in the LevelDB. Code is a port from the Bitcoin Core C++
    source:
        https://github.com/bitcoin/bitcoin/blob/v0.13.2/src/compressor.cpp#L161#L185

    :param x: Compressed amount to be decompressed.
    :type x: int
    :return: The decompressed amount of satoshi.
    :rtype: int
    """

    if x == 0:
        return 0
    x -= 1
    e = x % 10
    x /= 10
    if e < 9:
        d = (x % 9) + 1
        x /= 9
        n = x * 10 + d
    else:
        n = x + 1
    while e > 0:
        n *= 10
        e -= 1
    return n


def b128_encode(n):
    """ Performs the MSB base-128 encoding of a given value. Used to store variable integers (varints) in the LevelDB.
    The code is a port from the Bitcoin Core C++ source. Notice that the code is not exactly the same since the original
    one reads directly from the LevelDB.

    The encoding is used to store Satoshi amounts into the Bitcoin LevelDB (chainstate). Before encoding, values are
    compressed using txout_compress.

    The encoding can also be used to encode block height values into the format use in the LevelDB, however, those are
    encoded not compressed.

    Explanation can be found in:
        https://github.com/bitcoin/bitcoin/blob/v0.13.2/src/serialize.h#L307L329
    And code:
        https://github.com/bitcoin/bitcoin/blob/v0.13.2/src/serialize.h#L343#L358

    The MSB of every byte (x)xxx xxxx encodes whether there is another byte following or not. Hence, all MSB are set to
    one except from the very last. Moreover, one is subtracted from all but the last digit in order to ensure a
    one-to-one encoding. Hence, in order decode a value, the MSB is changed from 1 to 0, and 1 is added to the resulting
    value. Then, the value is multiplied to the respective 128 power and added to the rest.

    Examples:

        - 255 = 807F (0x80 0x7F) --> (1)000 0000 0111 1111 --> 0000 0001 0111 1111 --> 1 * 128 + 127 = 255
        - 4294967296 (2^32) = 8EFEFEFF (0x8E 0xFE 0xFE 0xFF 0x00) --> (1)000 1110 (1)111 1110 (1)111 1110 (1)111 1111
            0000 0000 --> 0000 1111 0111 1111 0111 1111 1000 0000 0000 0000 --> 15 * 128^4 + 127*128^3 + 127*128^2 +
            128*128 + 0 = 2^32


    :param n: Value to be encoded.
    :type n: int
    :return: The base-128 encoded value
    :rtype: hex str
    """

    l = 0
    tmp = []
    data = ""

    while True:
        tmp.append(n & 0x7F)
        if l != 0:
            tmp[l] |= 0x80
        if n <= 0x7F:
            break
        n = (n >> 7) - 1
        l += 1

    tmp.reverse()
    for i in tmp:
        data += format(i, '02x')
    return data


def b128_decode(data):
    """ Performs the MSB base-128 decoding of a given value. Used to decode variable integers (varints) from the LevelDB.
    The code is a port from the Bitcoin Core C++ source. Notice that the code is not exactly the same since the original
    one reads directly from the LevelDB.

    The decoding is used to decode Satoshi amounts stored in the Bitcoin LevelDB (chainstate). After decoding, values
    are decompressed using txout_decompress.

    The decoding can be also used to decode block height values stored in the LevelDB. In his case, values are not
    compressed.

    Original code can be found in:
        https://github.com/bitcoin/bitcoin/blob/v0.13.2/src/serialize.h#L360#L372

    Examples and further explanation can be found in b128_encode function.

    :param data: The base-128 encoded value to be decoded.
    :type data: hex str
    :return: The decoded value
    :rtype: int
    """

    n = 0
    i = 0
    while True:
        d = int(data[2 * i:2 * i + 2], 16)
        n = n << 7 | d & 0x7F
        if d & 0x80:
            n += 1
            i += 1
        else:
            return n


def parse_b128(utxo, offset=0):
    """ Parses a given serialized UTXO to extract a base-128 varint.

    :param utxo: Serialized UTXO from which the varint will be parsed.
    :type utxo: hex str
    :param offset: Offset where the beginning of the varint if located in the UTXO.
    :type offset: int
    :return: The extracted varint, and the offset of the byte located right after it.
    :rtype: hex str, int
    """

    data = utxo[offset:offset+2]
    offset += 2
    more_bytes = int(data, 16) & 0x80  # MSB b128 Varints have set the bit 128 for every byte but the last one,
    # indicating that there is an additional byte following the one being analyzed. If bit 128 of the byte being read is
    # not set, we are analyzing the last byte, otherwise, we should continue reading.
    while more_bytes:
        data += utxo[offset:offset+2]
        more_bytes = int(utxo[offset:offset+2], 16) & 0x80
        offset += 2

    return data, offset


def decode_utxo(coin, outpoint, version=0.15):
    """
    Decodes a LevelDB serialized UTXO for Bitcoin core v 0.15 onwards. The serialized format is defined in the Bitcoin
    Core source code as outpoint:coin.

    Outpoint structure is as follows: key | tx_hash | index.

    Where the key corresponds to b'C', or 43 in hex. The transaction hash in encoded in Little endian, and the index
    is a base128 varint. The corresponding Bitcoin Core source code can be found at:

    https://github.com/bitcoin/bitcoin/blob/ea729d55b4dbd17a53ced474a8457d4759cfb5a5/src/txdb.cpp#L40-L53

    On the other hand, a coin if formed by: code | value | out_type | script.

    Where code encodes the block height and whether the tx is coinbase or not, as 2*height + coinbase, the value is
    a txout_compressed base128 Varint, the out_type is also a base128 Varint, and the script is the remaining data.
    The corresponding Bitcoin Core soruce code can be found at:

    https://github.com/bitcoin/bitcoin/blob/6c4fecfaf7beefad0d1c3f8520bf50bb515a0716/src/coins.h#L58-L64

    :param coin: The coin to be decoded (extracted from the chainstate)
    :type coin: str
    :param outpoint: The outpoint to be decoded (extracted from the chainstate)
    :type outpoint: str
    :param version: Bitcoin Core version that created the chainstate LevelDB
    :return; The decoded UTXO.
    :rtype: dict
    """

    if 0.08 <= version < 0.15:
        return decode_utxo_v08_v014(coin)
    elif version < 0.08:
        raise Exception("The utxo decoder only works for version 0.08 onwards.")
    else:
        # First we will parse all the data encoded in the outpoint, that is, the transaction id and index of the utxo.
        # Check that the input data corresponds to a transaction.
        assert outpoint[:2] == '43'
        # Check the provided outpoint has at least the minimum length (1 byte of key code, 32 bytes tx id, 1 byte index)
        assert len(outpoint) >= 68
        # Get the transaction id (LE) by parsing the next 32 bytes of the outpoint.
        tx_id = outpoint[2:66]
        # Finally get the transaction index by decoding the remaining bytes as a b128 VARINT
        tx_index = b128_decode(outpoint[66:])

        # Once all the outpoint data has been parsed, we can proceed with the data encoded in the coin, that is, block
        # height, whether the transaction is coinbase or not, value, script type and script.
        # We start by decoding the first b128 VARINT of the provided data, that may contain 2*Height + coinbase
        code, offset = parse_b128(coin)
        code = b128_decode(code)
        height = code >> 1
        coinbase = code & 0x01

        # The next value in the sequence corresponds to the utxo value, the amount of Satoshi hold by the utxo. Data is
        # encoded as a B128 VARINT, and compressed using the equivalent to txout_compressor.
        data, offset = parse_b128(coin, offset)
        amount = txout_decompress(b128_decode(data))

        # Finally, we can obtain the data type by parsing the last B128 VARINT
        out_type, offset = parse_b128(coin, offset)
        out_type = b128_decode(out_type)

        if out_type in [0, 1]:
            data_size = 40  # 20 bytes
        elif out_type in [2, 3, 4, 5]:
            data_size = 66  # 33 bytes (1 byte for the type + 32 bytes of data)
            offset -= 2
        # Finally, if another value is found, it represents the length of the following data, which is uncompressed.
        else:
            data_size = (out_type - NSPECIALSCRIPTS) * 2  # If the data is not compacted, the out_type corresponds
            # to the data size adding the number os special scripts (nSpecialScripts).

        # And the remaining data corresponds to the script.
        script = coin[offset:]

        # Assert that the script hash the expected length
        assert len(script) == data_size

        # And to conclude, the output can be encoded. We will store it in a list for backward compatibility with the
        # previous decoder
        out = [{'amount': amount, 'out_type': out_type, 'data': script}]

    # Even though there is just one output, we will identify it as outputs for backward compatibility with the previous
    # decoder.
    return {'tx_id': tx_id, 'index': tx_index, 'coinbase': coinbase, 'outs': out, 'height': height}


def decode_utxo_v08_v014(utxo):
    """ Disclaimer: The internal structure of the chainstate LevelDB has been changed with Bitcoin Core v 0.15 release.
    Therefore, this function works for chainstate created with Bitcoin Core v 0.08-v0.14, for v 0.15 onwards use
    decode_utxo.

    Decodes a LevelDB serialized UTXO for Bitcoin core v 0.08 - v 0.14. The serialized format is defined in the Bitcoin
    Core source as follows:

     Serialized format:
     - VARINT(nVersion)
     - VARINT(nCode)
     - unspentness bitvector, for vout[2] and further; least significant byte first
     - the non-spent CTxOuts (via CTxOutCompressor)
     - VARINT(nHeight)

     The nCode value consists of:
     - bit 1: IsCoinBase()
     - bit 2: vout[0] is not spent
     - bit 4: vout[1] is not spent
     - The higher bits encode N, the number of non-zero bytes in the following bitvector.
        - In case both bit 2 and bit 4 are unset, they encode N-1, as there must be at
        least one non-spent output).

    VARINT refers to the CVarint used along the Bitcoin Core client, that is base128 encoding. A CTxOut contains the
    compressed amount of satoshi that the UTXO holds. That amount is encoded using the equivalent to txout_compress +
    b128_encode.

    :param utxo: UTXO to be decoded (extracted from the chainstate)
    :type utxo: hex str
    :return; The decoded UTXO.
    :rtype: dict
    """

    # Version is extracted from the first varint of the serialized utxo
    version, offset = parse_b128(utxo)
    version = b128_decode(version)

    # The next MSB base 128 varint is parsed to extract both is the utxo is coin base (first bit) and which of the
    # outputs are not spent.
    code, offset = parse_b128(utxo, offset)
    code = b128_decode(code)
    coinbase = code & 0x01

    # Check if the first two outputs are spent
    vout = [(code | 0x01) & 0x02, (code | 0x01) & 0x04]

    # The higher bits of the current byte (from the fourth onwards) encode n, the number of non-zero bytes of
    # the following bitvector. If both vout[0] and vout[1] are spent (v[0] = v[1] = 0) then the higher bits encodes n-1,
    # since there should be at least one non-spent output.
    if not vout[0] and not vout[1]:
        n = (code >> 3) + 1
        vout = []
    else:
        n = code >> 3
        vout = [i for i in xrange(len(vout)) if vout[i] is not 0]

    # If n is set, the encoded value contains a bitvector. The following bytes are parsed until n non-zero bytes have
    # been extracted. (If a 00 is found, the parsing continues but n is not decreased)
    if n > 0:
        bitvector = ""
        while n:
            data = utxo[offset:offset+2]
            if data != "00":
                n -= 1
            bitvector += data
            offset += 2

        # Once the value is parsed, the endianness of the value is switched from LE to BE and the binary representation
        # of the value is checked to identify the non-spent output indexes.
        bin_data = format(int(change_endianness(bitvector), 16), '0'+str(n*8)+'b')[::-1]

        # Every position (i) with a 1 encodes the index of a non-spent output as i+2, since the two first outs (v[0] and
        # v[1] has been already counted)
        # (e.g: 0440 (LE) = 4004 (BE) = 0100 0000 0000 0100. It encodes outs 4 (i+2 = 2+2) and 16 (i+2 = 14+2).
        extended_vout = [i+2 for i in xrange(len(bin_data))
                         if bin_data.find('1', i) == i]  # Finds the index of '1's and adds 2.

        # Finally, the first two vouts are included to the list (if they are non-spent).
        vout += extended_vout

    # Once the number of outs and their index is known, they could be parsed.
    outs = []
    for i in vout:
        # The satoshi amount is parsed, decoded and decompressed.
        data, offset = parse_b128(utxo, offset)
        amount = txout_decompress(b128_decode(data))
        # The output type is also parsed.
        out_type, offset = parse_b128(utxo, offset)
        out_type = b128_decode(out_type)
        # Depending on the type, the length of the following data will differ.  Types 0 and 1 refers to P2PKH and P2SH
        # encoded outputs. They are always followed 20 bytes of data, corresponding to the hash160 of the address (in
        # P2PKH outputs) or to the scriptHash (in P2PKH). Notice that the leading and tailing opcodes are not included.
        # If 2-5 is found, the following bytes encode a public key. The first byte in this case should be also included,
        # since it determines the format of the key.
        if out_type in [0, 1]:
            data_size = 40  # 20 bytes
        elif out_type in [2, 3, 4, 5]:
            data_size = 66  # 33 bytes (1 byte for the type + 32 bytes of data)
            offset -= 2
        # Finally, if another value is found, it represents the length of the following data, which is uncompressed.
        else:
            data_size = (out_type - NSPECIALSCRIPTS) * 2  # If the data is not compacted, the out_type corresponds
            # to the data size adding the number os special scripts (nSpecialScripts).

        # And finally the address (the hash160 of the public key actually)
        data, offset = utxo[offset:offset+data_size], offset + data_size
        outs.append({'index': i, 'amount': amount, 'out_type': out_type, 'data': data})

    # Once all the outs are processed, the block height is parsed
    height, offset = parse_b128(utxo, offset)
    height = b128_decode(height)
    # And the length of the serialized utxo is compared with the offset to ensure that no data remains unchecked.
    assert len(utxo) == offset

    return {'version': version, 'coinbase': coinbase, 'outs': outs, 'height': height}


def display_decoded_utxo(decoded_utxo):
    """ Displays the information extracted from a decoded UTXO from the chainstate.

    :param decoded_utxo: Decoded UTXO from the chainstate
    :type decoded_utxo: dict
    :return: None
    :rtype: None
    """

    print "version: " + str(decoded_utxo['version'])
    print "isCoinbase: " + str(decoded_utxo['coinbase'])

    outs = decoded_utxo['outs']
    print "Number of outputs: " + str(len(outs))
    for out in outs:
        print "vout[" + str(out['index']) + "]:"
        print "\tSatoshi amount: " + str(out['amount'])
        print "\tOutput code type: " + out['out_type']
        print "\tHash160 (Address): " + out['address']

    print "Block height: " + str(decoded_utxo['height'])


def parse_ldb(fout_name, fin_name='chainstate', version=0.15):
    largestBlock = 0
    smallestBlock = 100000000 # should not be bigger than this number
    """
    Parsed data from the chainstate LevelDB and stores it in a output file.
    :param fout_name: Name of the file to output the data.
    :type fout_name: str
    :param fin_name: Name of the LevelDB folder (chainstate by default)
    :type fin_name: str
    :param version: Bitcoin Core client version. Determines the prefix of the transaction entries.
    :param version: float
    :return: None
    :rtype: None
    """
    if 0.08 <= version < 0.15:
        prefix = b'c'
    elif version < 0.08:
        raise Exception("The utxo decoder only works for version 0.08 onwards.")
    else:
        prefix = b'C'

    # Output file
    fout = open(CFG.data_path + fout_name, 'wb')

    # Open the LevelDB
    db = plyvel.DB(CFG.btc_core_path + "/" + fin_name, compression=None)  # Change with path to chainstate

    # Load obfuscation key (if it exists)
    o_key = db.get((unhexlify("0e00") + "obfuscate_key"))

    # If the key exists, the leading byte indicates the length of the key (8 byte by default). If there is no key,
    # 8-byte zeros are used (since the key will be XORed with the given values).
    if o_key is not None:
        o_key = hexlify(o_key)[2:]

    # For every UTXO (identified with a leading 'c'), the key (tx_id) and the value (encoded utxo) is displayed.
    # UTXOs are obfuscated using the obfuscation key (o_key), in order to get them non-obfuscated, a XOR between the
    # value and the key (concatenated until the length of the value is reached) if performed).
    std_types = [0, 1, 2, 3, 4, 5]
    non_std_only = False
    count_p2sh=False
    counter = 0

    for key, o_value in db.iterator(prefix=prefix):
        if o_key is not None:
            value = deobfuscate_value(o_key, hexlify(o_value))
        else:
            value = hexlify(o_value)
        hexed_key = hexlify(key)
        # fout.write(ujson.dumps({"key":  hexlify(key), "value": value}) + "\n")
        # utxo = decode_utxo(value, None, version)
        utxo = decode_utxo(value, hexlify(key), version)
        tx_id = change_endianness(utxo.get('tx_id'))
        for out in utxo.get("outs"):
            # Checks whether we are looking for every type of UTXO or just for non-standard ones.
            if not non_std_only or (non_std_only and out["out_type"] not in std_types
                                    and not check_multisig(out['data'])):

                # Calculates the dust threshold for every UTXO value and every fee per byte ratio between min and max.
                min_size = get_min_input_size(out, utxo["height"], count_p2sh)
                dust = 0
                lm = 0

                if min_size > 0:
                    raw_dust = out["amount"] / float(3 * min_size)
                    raw_lm = out["amount"] / float(min_size)

                    dust = roundup_rate(raw_dust, FEE_STEP)
                    lm = roundup_rate(raw_lm, FEE_STEP)

                # Adds multisig type info
                if out["out_type"] == 0:
                    non_std_type = "std"
                else:
                    continue
                    # non_std_type = check_multisig_type(out["data"])  #don't check multisig for now

                # Builds the output dictionary
                result = {"tx_id": tx_id,              # transaction id
                          "tx_height": utxo["height"], # block num
                          "data" : out["data"],        # public key hash
                          "amount" : out["amount"]     # amount
                          # ,
                          # "utxo_data_len": len(out["data"]) / 2,
                          # "dust": dust,
                          # "loss_making": lm,
                          # "non_std_type": non_std_type
                          }
                # Index added at the end when updated the result with the out, since the index is not part of the
                # encoded data anymore (coin) but of the entry identifier (outpoint), we add it manually.
                if version >= 0.15:
                    result['index'] = utxo['index']
                    # result['register_len'] = len(value) / 2 + len(hexed_key) / 2
                # Updates the dictionary with the remaining data from out, and stores it in disk.
                # result.update(out)
                # print result
                # print unhexlify(result['tx_id'])
                # print result["amount"]
                
                ## Raw data
                fout.write(unhexlify(result['tx_id']))
                fout.write(struct.pack('<Q', result['amount']))
                fout.write(struct.pack('<I', result['tx_height']))
                fout.write(unhexlify(result['data']))
                fout.write(struct.pack('<I', result['index']))
                if smallestBlock >result['tx_height']:
                    smallestBlock = result['tx_height']
                if largestBlock < result['tx_height']:
                    largestBlock = result['tx_height']
                ## Json
                # fout.write(ujson.dumps(result) + '\n')
        counter+=1
        if counter % 1000000 == 0:
            print "[>] Currently at ", counter
    db.close()
    print "[+] Finished parsing raw utxo"
    print "[>] Largest block  :", largestBlock
    print "[>] Smallest block :", smallestBlock


def accumulate_dust_lm(fin_name, fout_name="dust.json"):
    """
    Accumulates all the dust / lm of a given parsed utxo file (from utxo_dump function).

    :param fin_name: Input file name, from where data wil be loaded.
    :type fin_name: str
    :param fout_name: Output file name, where data will be stored.
    :type fout_name: str
    :return: None
    :rtype: None
    """

    # Dust calculation
    # Input file
    fin = open(CFG.data_path + fin_name, 'r')

    dust = {fee_per_byte: 0 for fee_per_byte in range(MIN_FEE_PER_BYTE, MAX_FEE_PER_BYTE, FEE_STEP)}
    value_dust = deepcopy(dust)
    data_len_dust = deepcopy(dust)

    lm = deepcopy(dust)
    value_lm = deepcopy(dust)
    data_len_lm = deepcopy(dust)

    total_utxo = 0
    total_value = 0
    total_data_len = 0

    for line in fin:
        data = ujson.loads(line[:-1])

        # If the UTXO is dust for the checked range, we increment the dust count, dust value and dust length for the
        # given threshold.
        if 0 < data['dust'] < MAX_FEE_PER_BYTE:
            if data['dust'] < MIN_FEE_PER_BYTE:
                rate = MIN_FEE_PER_BYTE
            elif MIN_FEE_PER_BYTE <= data['dust']:
                rate = data['dust']

            dust[rate] += 1
            value_dust[rate] += data["amount"]
            data_len_dust[rate] += data["utxo_data_len"]

        # Same with non-profitable outputs.
        if 0 < data['loss_making'] < MAX_FEE_PER_BYTE:
            if data['loss_making'] < MIN_FEE_PER_BYTE:
                rate = MIN_FEE_PER_BYTE
            elif MIN_FEE_PER_BYTE <= data['loss_making']:
                rate = data['loss_making']

            lm[rate] += 1
            value_lm[rate] += data["amount"]
            data_len_lm[rate] += data["utxo_data_len"]

        # And we increase the total counters for each read utxo.
        total_utxo = total_utxo + 1
        total_value += data["amount"]
        total_data_len += data["utxo_data_len"]

    fin.close()

    # Moreover, since if an output is dust/non-profitable for a given threshold, it will also be for every other step
    # onwards, we accumulate the result of a given step with the accumulated value from the previous step.
    for fee_per_byte in range(MIN_FEE_PER_BYTE+FEE_STEP, MAX_FEE_PER_BYTE, FEE_STEP):
        dust[fee_per_byte] += dust[fee_per_byte - FEE_STEP]
        value_dust[fee_per_byte] += value_dust[fee_per_byte - FEE_STEP]
        data_len_dust[fee_per_byte] += data_len_dust[fee_per_byte - FEE_STEP]

        lm[fee_per_byte] += lm[fee_per_byte - FEE_STEP]
        value_lm[fee_per_byte] += value_lm[fee_per_byte - FEE_STEP]
        data_len_lm[fee_per_byte] += data_len_lm[fee_per_byte - FEE_STEP]

    # Finally we create the output with the accumulated data and store it.
    data = {"dust_utxos": dust, "dust_value": value_dust, "dust_data_len": data_len_dust,
            "lm_utxos": lm, "lm_value": value_lm, "lm_data_len": data_len_lm,
            "total_utxos": total_utxo, "total_value": total_value, "total_data_len": total_data_len}

    # Store dust calculation in a file.
    out = open(CFG.data_path + fout_name, 'w')
    out.write(ujson.dumps(data))
    out.close()


def check_multisig(script, std=True):
    """
    Checks whether a given script is a multisig one. By default, only standard multisig script are accepted.

    :param script: The script to be checked.
    :type script: str
    :param std: Whether the script is standard or not.
    :type std: bool
    :return: True if the script is multisig (under the std restrictions), False otherwise.
    :rtype: bool
    """

    if std:
        # Standard bare Pay-to-multisig only accepts up to 3-3.
        r = range(81, 83)
    else:
        # m-of-n combination is valid up to 20.
        r = range(84, 101)

    if int(script[:2], 16) in r and script[2:4] in ["21", "41"] and script[-2:] == "ae":
        return True
    else:
        return False


def check_multisig_type(script):
    """
    Checks whether a given script is a multisig one. If it is multisig, return type (m and n values).

    :param script: The script to be checked.
    :type script: str
    :return: "multisig-m-n" or False
    """

    if len(OutputScript.deserialize(script).split()) > 2:
        # TODO: should we be more restrictive?
        m = OutputScript.deserialize(script).split()[0]
        n = OutputScript.deserialize(script).split()[-2]
        op_multisig = OutputScript.deserialize(script).split()[-1]

        if op_multisig == "OP_CHECKMULTISIG" and script[2:4] in ["21", "41"]:
            return "multisig-"+ str(m) + "-" + str(n)

    return False


def check_opreturn(script):
    """
    Checks whether a given script is an OP_RETURN one.

    Warning: there should NOT be any OP_RETURN output in the UTXO set.

    :param script: The script to be checked.
    :type script: str
    :return: True if the script is an OP_RETURN, False otherwise.
    :rtype: bool
    """
    op_return_opcode = 0x6a
    return int(script[:2], 16) == op_return_opcode


def get_min_input_size(out, height, count_p2sh=False):
    """
    Computes the minimum size an input created by a given output type (parsed from the chainstate) will have.
    The size is computed in two parts, a fixed size that is non type dependant, and a variable size which
    depends on the output type.

    :param out: Output to be analyzed.
    :type out: dict
    :param height: Block height where the utxo was created. Used to set P2PKH min_size.
    :type height: int
    :param count_p2sh: Whether P2SH should be taken into account.
    :type count_p2sh: bool
    :return: The minimum input size of the given output type.
    :rtype: int
    """

    out_type = out["out_type"]
    script = out["data"]

    # Fixed size
    prev_tx_id = 32
    prev_out_index = 4
    nSequence = 4

    fixed_size = prev_tx_id + prev_out_index + nSequence

    # Variable size (depending on scripSig):
    # Public key size can be either 33 or 65 bytes, depending on whether the key is compressed or uncompressed. We wil
    # make them fall in one of the categories depending on the block height in which the transaction was included.
    #
    # Signatures size is contained between 71-73 bytes depending on the size of the S and R components of the signature.
    # Since we are looking for the minimum size, we will consider all signatures to be 71-byte long in order to define
    # a lower bound.

    if out_type is 0:
        # P2PKH
        # Bitcoin core starts using compressed pk in version (0.6.0, 30/03/12, around block height 173480)
        if height < 173480:
            # uncompressed keys
            scriptSig = 138  # PUSH sig (1 byte) + sig (71 bytes) + PUSH pk (1 byte) + uncompressed pk (65 bytes)
        else:
            # compressed keys
            scriptSig = 106  # PUSH sig (1 byte) + sig (71 bytes) + PUSH pk (1 byte) + compressed pk (33 bytes)
        scriptSig_len = 1
    elif out_type is 1:
        # P2SH
        # P2SH inputs can have arbitrary length. Defining the length of the original script by just knowing the hash
        # is infeasible. Two approaches can be followed in this case. The first one consists on considering P2SH
        # by defining the minimum length a script of such type could have. The other approach will be ignoring such
        # scripts when performing the dust calculation.
        if count_p2sh:
            # If P2SH UTXOs are considered, the minimum script that can be created has only 1 byte (OP_1 for example)
            scriptSig = 1
            scriptSig_len = 1
        else:
            # Otherwise, we will define the length as 0 and skip such scripts for dust calculation.
            scriptSig = -fixed_size
            scriptSig_len = 0
    elif out_type in [2, 3, 4, 5]:
        # P2PK
        # P2PK requires a signature and a push OP_CODE to push the signature into the stack. The format of the public
        # key (compressed or uncompressed) does not affect the length of the signature.
        scriptSig = 72  # PUSH sig (1 byte) + sig (71 bytes)
        scriptSig_len = 1
    else:
        # P2MS
        if check_multisig(script):
            # Multisig can be 15-15 at most.
            req_sigs = int(script[:2], 16) - 80  # OP_1 is hex 81
            scriptSig = 1 + (req_sigs * 72)  # OP_0 (1 byte) + 72 bytes per sig (PUSH sig (1 byte) + sig (71 bytes))
            scriptSig_len = int(ceil(scriptSig / float(256)))
        else:
            # All other types (non-standard outs)
            scriptSig = -fixed_size - 1  # Those scripts are marked with length -1 and skipped in dust calculation.
            scriptSig_len = 0

    var_size = scriptSig_len + scriptSig

    return fixed_size + var_size


def get_utxo(tx_id, index, fin_name='chainstate', version=0.15):
    """
    Gets a UTXO from the chainstate identified by a given transaction id and index.
    If the requested UTXO does not exist, return None.

    :param tx_id: Transaction ID that identifies the UTXO you are looking for.
    :type tx_id: str
    :param index: Index that identifies the specific output.
    :type index: int
    :param fin_name: Name of the LevelDB folder (chainstate by default)
    :type fin_name: str
    :param version: Bitcoin Core client version. Determines the prefix of the transaction entries.
    :param version: float
    :return: A outpoint:coin pair representing the requested UTXO
    :rtype: str, str
    """

    if 0.08 <= version < 0.15:
        prefix = b'c'
        outpoint = prefix + unhexlify(tx_id)
    elif version < 0.08:
        raise Exception("The utxo decoder only works for version 0.08 onwards.")
    else:
        prefix = b'C'
        outpoint = prefix + unhexlify(tx_id + b128_encode(index))

    # Open the LevelDB
    db = plyvel.DB(CFG.btc_core_path + "/" + fin_name, compression=None)  # Change with path to chainstate

    # Load obfuscation key (if it exists)
    o_key = db.get((unhexlify("0e00") + "obfuscate_key"))

    # If the key exists, the leading byte indicates the length of the key (8 byte by default). If there is no key,
    # 8-byte zeros are used (since the key will be XORed with the given values).
    if o_key is not None:
        o_key = hexlify(o_key)[2:]

    coin = db.get(outpoint)

    if coin is not None and o_key is not None:
        coin = deobfuscate_value(o_key, coin)

    db.close()

    # ToDO: Add code to return a single output for 0.8 - 0.14

    return hexlify(outpoint), coin


def deobfuscate_value(obfuscation_key, value):
    """
    De-obfuscate a given value parsed from the chainstate.

    :param obfuscation_key: Key used to obfuscate the given value (extracted from the chainstate).
    :type obfuscation_key: str
    :param value: Obfuscated value.
    :type value: str
    :return: The de-obfuscated value.
    :rtype: str.
    """

    l_value = len(value)
    l_obf = len(obfuscation_key)

    # Get the extended obfuscation key by concatenating the obfuscation key with itself until it is as large as the
    # value to be de-obfuscated.
    if l_obf < l_value:
        extended_key = (obfuscation_key * ((l_value / l_obf) + 1))[:l_value]
    else:
        extended_key = obfuscation_key[:l_value]

    r = format(int(value, 16) ^ int(extended_key, 16), 'x')

    # In some cases, the obtained value could be 1 byte smaller than the original, since the leading 0 is dropped off
    # when the formatting.
    if len(r) is l_value-1:
        r = r.zfill(l_value)

    assert len(value) == len(r)

    return r


def roundup_rate(fee_rate, fee_step=FEE_STEP):

    """
    Rounds up a given fee rate to the nearest fee_step (FEE_STEP by default). If the rounded value it the value itself,
    adds fee_step, assuring that the returning rate is always bigger than the given one.

    :param fee_rate: Fee rate to be rounded up.
    :type fee_rate: float
    :param fee_step: Value at which fee_rate will be round up (FEE_STEP by default)
    :type fee_step: int
    :return: The rounded up fee_rate.
    :rtype: int
    """

    # If the value to be rounded is already multiple of the fee step, we just add another step. Otherwise the value
    # is rounded up.
    if (fee_rate % fee_step) == 0.0:
        rate = int(fee_rate + fee_step)
    else:
        rate = int(ceil(fee_rate / float(10))) * 10

    # # If the rounded up value is
    # if rate >= MAX_FEE_PER_BYTE:
    #     rate = 0

    return rate

