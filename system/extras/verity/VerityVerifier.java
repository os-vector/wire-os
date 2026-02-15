/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.verity;

import java.io.File;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.lang.Math;
import java.lang.Process;
import java.lang.Runtime;
import java.math.BigInteger;
import java.security.KeyFactory;
import java.security.MessageDigest;
import java.security.PublicKey;
import java.security.Security;
import java.security.cert.X509Certificate;
import java.security.interfaces.RSAPublicKey;
import java.security.spec.RSAPublicKeySpec;
import java.util.ArrayList;
import java.util.Arrays;
import javax.xml.bind.DatatypeConverter;
import org.bouncycastle.jce.provider.BouncyCastleProvider;

public class VerityVerifier {

    private ArrayList<Integer> hashBlocksLevel;
    private byte[] hashTree;
    private byte[] rootHash;
    private byte[] salt;
    private byte[] signature;
    private byte[] table;
    private File image;
    private int blockSize;
    private int hashBlockSize;
    private int hashOffsetForData;
    private int hashSize;
    private int hashTreeSize;
    private long hashStart;
    private long imageSize;
    private MessageDigest digest;

    private static final int EXT4_SB_MAGIC = 0xEF53;
    private static final int EXT4_SB_OFFSET = 0x400;
    private static final int EXT4_SB_OFFSET_MAGIC = EXT4_SB_OFFSET + 0x38;
    private static final int EXT4_SB_OFFSET_LOG_BLOCK_SIZE = EXT4_SB_OFFSET + 0x18;
    private static final int EXT4_SB_OFFSET_BLOCKS_COUNT_LO = EXT4_SB_OFFSET + 0x4;
    private static final int EXT4_SB_OFFSET_BLOCKS_COUNT_HI = EXT4_SB_OFFSET + 0x150;
    private static final int MINCRYPT_OFFSET_MODULUS = 0x8;
    private static final int MINCRYPT_OFFSET_EXPONENT = 0x208;
    private static final int MINCRYPT_MODULUS_SIZE = 0x100;
    private static final int MINCRYPT_EXPONENT_SIZE = 0x4;
    private static final int VERITY_FIELDS = 10;
    private static final int VERITY_MAGIC = 0xB001B001;
    private static final int VERITY_SIGNATURE_SIZE = 256;
    private static final int VERITY_VERSION = 0;

    public VerityVerifier(String fname) throws Exception {
        digest = MessageDigest.getInstance("SHA-256");
        hashSize = digest.getDigestLength();
        hashBlocksLevel = new ArrayList<Integer>();
        hashTreeSize = -1;
        openImage(fname);
        readVerityData();
    }

    /**
     * Reverses the order of bytes in a byte array
     * @param value Byte array to reverse
     */
    private static byte[] reverse(byte[] value) {
        for (int i = 0; i < value.length / 2; i++) {
            byte tmp = value[i];
            value[i] = value[value.length - i - 1];
            value[value.length - i - 1] = tmp;
        }

        return value;
    }

    /**
     * Converts a 4-byte little endian value to a Java integer
     * @param value Little endian integer to convert
     */
    private static int fromle(int value) {
        byte[] bytes = ByteBuffer.allocate(4).putInt(value).array();
        return ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).getInt();
    }

     /**
     * Converts a 2-byte little endian value to Java a integer
     * @param value Little endian short to convert
     */
    private static int fromle(short value) {
        return fromle(value << 16);
    }

    /**
     * Reads a 2048-bit RSA public key saved in mincrypt format, and returns
     * a Java PublicKey for it.
     * @param fname Name of the mincrypt public key file
     */
    private static PublicKey getMincryptPublicKey(String fname) throws Exception {
        try (RandomAccessFile key = new RandomAccessFile(fname, "r")) {
            byte[] binaryMod = new byte[MINCRYPT_MODULUS_SIZE];
            byte[] binaryExp = new byte[MINCRYPT_EXPONENT_SIZE];

            key.seek(MINCRYPT_OFFSET_MODULUS);
            key.readFully(binaryMod);

            key.seek(MINCRYPT_OFFSET_EXPONENT);
            key.readFully(binaryExp);

            BigInteger modulus  = new BigInteger(1, reverse(binaryMod));
            BigInteger exponent = new BigInteger(1, reverse(binaryExp));

            RSAPublicKeySpec spec = new RSAPublicKeySpec(modulus, exponent);
            KeyFactory factory = KeyFactory.getInstance("RSA");
            return factory.generatePublic(spec);
        }
    }

    /**
     * Unsparses a sparse image into a temporary file and returns a
     * handle to the file
     * @param fname Path to a sparse image file
     */
     private void openImage(String fname) throws Exception {
        image = File.createTempFile("system", ".raw");
        image.deleteOnExit();

        Process p = Runtime.getRuntime().exec("simg2img " + fname +
                            " " + image.getAbsoluteFile());

        p.waitFor();
        if (p.exitValue() != 0) {
            throw new IllegalArgumentException("Invalid image: failed to unsparse");
        }
    }

    /**
     * Reads the ext4 superblock and calculates the size of the system image,
     * after which we should find the verity metadata
     * @param img File handle to the image file
     */
    public static long getMetadataPosition(RandomAccessFile img)
            throws Exception {
        img.seek(EXT4_SB_OFFSET_MAGIC);
        int magic = fromle(img.readShort());

        if (magic != EXT4_SB_MAGIC) {
            throw new IllegalArgumentException("Invalid image: not a valid ext4 image");
        }

        img.seek(EXT4_SB_OFFSET_BLOCKS_COUNT_LO);
        long blocksCountLo = fromle(img.readInt());

        img.seek(EXT4_SB_OFFSET_LOG_BLOCK_SIZE);
        long logBlockSize = fromle(img.readInt());

        img.seek(EXT4_SB_OFFSET_BLOCKS_COUNT_HI);
        long blocksCountHi = fromle(img.readInt());

        long blockSizeBytes = 1L << (10 + logBlockSize);
        long blockCount = (blocksCountHi << 32) + blocksCountLo;
        return blockSizeBytes * blockCount;
    }

    /**
     * Calculates the size of the verity hash tree based on the image size
     */
    private int calculateHashTreeSize() {
        if (hashTreeSize > 0) {
            return hashTreeSize;
        }

        int totalBlocks = 0;
        int hashes = (int) (imageSize / blockSize);

        hashBlocksLevel.clear();

        do {
            hashBlocksLevel.add(0, hashes);

            int hashBlocks =
                (int) Math.ceil((double) hashes * hashSize / hashBlockSize);

            totalBlocks += hashBlocks;

            hashes = hashBlocks;
        } while (hashes > 1);

        hashTreeSize = totalBlocks * hashBlockSize;
        return hashTreeSize;
    }

    /**
     * Parses the verity mapping table and reads the hash tree from
     * the image file
     * @param img Handle to the image file
     * @param table Verity mapping table
     */
    private void readHashTree(RandomAccessFile img, byte[] table)
            throws Exception {
        String tableStr = new String(table);
        String[] fields = tableStr.split(" ");

        if (fields.length != VERITY_FIELDS) {
            throw new IllegalArgumentException("Invalid image: unexpected number of fields "
                    + "in verity mapping table (" + fields.length + ")");
        }

        String hashVersion = fields[0];

        if (!"1".equals(hashVersion)) {
            throw new IllegalArgumentException("Invalid image: unsupported hash format");
        }

        String alg = fields[7];

        if (!"sha256".equals(alg)) {
            throw new IllegalArgumentException("Invalid image: unsupported hash algorithm");
        }

        blockSize = Integer.parseInt(fields[3]);
        hashBlockSize = Integer.parseInt(fields[4]);

        int blocks = Integer.parseInt(fields[5]);
        int start = Integer.parseInt(fields[6]);

        if (imageSize != (long) blocks * blockSize) {
            throw new IllegalArgumentException("Invalid image: size mismatch in mapping "
                    + "table");
        }

        rootHash = DatatypeConverter.parseHexBinary(fields[8]);
        salt = DatatypeConverter.parseHexBinary(fields[9]);

        hashStart = (long) start * blockSize;
        img.seek(hashStart);

        int treeSize = calculateHashTreeSize();

        hashTree = new byte[treeSize];
        img.readFully(hashTree);
    }

    /**
     * Reads verity data from the image file
     */
    private void readVerityData() throws Exception {
        try (RandomAccessFile img = new RandomAccessFile(image, "r")) {
            imageSize = getMetadataPosition(img);
            img.seek(imageSize);

            int magic = fromle(img.readInt());

            if (magic != VERITY_MAGIC) {
                throw new IllegalArgumentException("Invalid image: verity metadata not found");
            }

            int version = fromle(img.readInt());

            if (version != VERITY_VERSION) {
                throw new IllegalArgumentException("Invalid image: unknown metadata version");
            }

            signature = new byte[VERITY_SIGNATURE_SIZE];
            img.readFully(signature);

            int tableSize = fromle(img.readInt());

            table = new byte[tableSize];
            img.readFully(table);

            readHashTree(img, table);
        }
    }

    /**
     * Reads and validates verity metadata, and checks the signature against the
     * given public key
     * @param key Public key to use for signature verification
     */
    public boolean verifyMetaData(PublicKey key)
            throws Exception {
       return Utils.verify(key, table, signature,
                   Utils.getSignatureAlgorithmIdentifier(key));
    }

    /**
     * Hashes a block of data using a salt and checks of the results are expected
     * @param hash The expected hash value
     * @param data The data block to check
     */
    private boolean checkBlock(byte[] hash, byte[] data) {
        digest.reset();
        digest.update(salt);
        digest.update(data);
        return Arrays.equals(hash, digest.digest());
    }

    /**
     * Verifies the root hash and the first N-1 levels of the hash tree
     */
    private boolean verifyHashTree() throws Exception {
        int hashOffset = 0;
        int dataOffset = hashBlockSize;

        if (!checkBlock(rootHash, Arrays.copyOfRange(hashTree, 0, hashBlockSize))) {
            System.err.println("Root hash mismatch");
            return false;
        }

        for (int level = 0; level < hashBlocksLevel.size() - 1; level++) {
            int blocks = hashBlocksLevel.get(level);

            for (int i = 0; i < blocks; i++) {
                byte[] hashBlock = Arrays.copyOfRange(hashTree,
                        hashOffset + i * hashSize,
                        hashOffset + i * hashSize + hashSize);

                byte[] dataBlock = Arrays.copyOfRange(hashTree,
                        dataOffset + i * hashBlockSize,
                        dataOffset + i * hashBlockSize + hashBlockSize);

                if (!checkBlock(hashBlock, dataBlock)) {
                    System.err.printf("Hash mismatch at tree level %d, block %d\n", level, i);
                    return false;
                }
            }

            hashOffset = dataOffset;
            hashOffsetForData = dataOffset;
            dataOffset += blocks * hashBlockSize;
        }

        return true;
    }

    /**
     * Validates the image against the hash tree
     */
    public boolean verifyData() throws Exception {
        if (!verifyHashTree()) {
            return false;
        }

        try (RandomAccessFile img = new RandomAccessFile(image, "r")) {
            byte[] dataBlock = new byte[blockSize];
            int hashOffset = hashOffsetForData;

            for (int i = 0; (long) i * blockSize < imageSize; i++) {
                byte[] hashBlock = Arrays.copyOfRange(hashTree,
                        hashOffset + i * hashSize,
                        hashOffset + i * hashSize + hashSize);

                img.readFully(dataBlock);

                if (!checkBlock(hashBlock, dataBlock)) {
                    System.err.printf("Hash mismatch at block %d\n", i);
                    return false;
                }
            }
        }

        return true;
    }

    /**
     * Verifies the integrity of the image and the verity metadata
     * @param key Public key to use for signature verification
     */
    public boolean verify(PublicKey key) throws Exception {
        return (verifyMetaData(key) && verifyData());
    }

    public static void main(String[] args) throws Exception {
        Security.addProvider(new BouncyCastleProvider());
        PublicKey key = null;

        if (args.length == 3 && "-mincrypt".equals(args[1])) {
            key = getMincryptPublicKey(args[2]);
        } else if (args.length == 2) {
            X509Certificate cert = Utils.loadPEMCertificate(args[1]);
            key = cert.getPublicKey();
        } else {
            System.err.println("Usage: VerityVerifier <sparse.img> <certificate.x509.pem> | -mincrypt <mincrypt_key>");
            System.exit(1);
        }

        VerityVerifier verifier = new VerityVerifier(args[0]);

        try {
            if (verifier.verify(key)) {
                System.err.println("Signature is VALID");
                System.exit(0);
            }
        } catch (Exception e) {
            e.printStackTrace(System.err);
        }

        System.exit(1);
    }
}
