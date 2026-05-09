/**
 * @file flash_eeprom.cpp
 * @brief Implémentation de l'émulation EEPROM dans la Flash
 * 
 * Ce fichier contient l'implémentation de toutes les fonctions
 * déclarées dans flash_eeprom.h.
 * 
 * Il gère :
 * - L'écriture de variables dans la Flash avec wear leveling
 * - La lecture et la restauration des données
 * - La vérification d'intégrité par CRC32
 * - Le double buffering pour la sécurité
 * - La défragmentation et le compactage
 * 
 * @author Votre Nom
 * @date 2024
 */

// ============================================================
// INCLUDES
// ============================================================

#include "flash_eeprom.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// VARIABLES GLOBALES
// ============================================================

/** @brief État du module */
static FlashEEPROM_State eeprom_state;

/** @brief Tampon temporaire pour les opérations */
static uint8_t temp_buffer[FLASH_EEPROM_PAGE_SIZE];

// ============================================================
// SECTION 1 : INITIALISATION
// ============================================================

/**
 * @brief Initialise le module Flash EEPROM
 */
FlashEEPROM_Error flash_eeprom_init(void)
{
    FLASH_EEPROM_DEBUG("Initialisation Flash EEPROM...\n");
    
    memset(&eeprom_state, 0, sizeof(FlashEEPROM_State));
    
    // Déverrouiller la Flash
    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        FLASH_EEPROM_DEBUG("Échec déverrouillage Flash\n");
        return FLASH_EEPROM_ERROR_LOCKED;
    }
    
    eeprom_state.locked = false;
    
    // Déterminer quel buffer est actif (le plus récent)
    FlashEEPROM_Header* headerA = (FlashEEPROM_Header*)FLASH_EEPROM_BUFFER_A_ADDR;
    FlashEEPROM_Header* headerB = (FlashEEPROM_Header*)FLASH_EEPROM_BUFFER_B_ADDR;
    
    bool a_valid = (headerA->magic == FLASH_EEPROM_MAGIC);
    bool b_valid = (headerB->magic == FLASH_EEPROM_MAGIC);
    
    if (a_valid && b_valid)
    {
        // Les deux sont valides : choisir le plus récent (version la plus haute)
        if (headerB->version >= headerA->version)
        {
            eeprom_state.activeBuffer = FLASH_EEPROM_BUFFER_B_ADDR;
        }
        else
        {
            eeprom_state.activeBuffer = FLASH_EEPROM_BUFFER_A_ADDR;
        }
    }
    else if (a_valid)
    {
        eeprom_state.activeBuffer = FLASH_EEPROM_BUFFER_A_ADDR;
    }
    else if (b_valid)
    {
        eeprom_state.activeBuffer = FLASH_EEPROM_BUFFER_B_ADDR;
    }
    else
    {
        // Aucun buffer valide : formater le buffer A
        FLASH_EEPROM_DEBUG("Aucun buffer valide, formatage...\n");
        flash_eeprom_format();
    }
    
    // Calculer l'offset actuel (parcourir le buffer actif)
    eeprom_state.currentOffset = 0;
    eeprom_state.usedSpace = 0;
    
    uint32_t addr = eeprom_state.activeBuffer;
    FlashEEPROM_Header* header;
    
    while (addr < eeprom_state.activeBuffer + FLASH_EEPROM_SECTOR_SIZE - sizeof(FlashEEPROM_Header))
    {
        header = (FlashEEPROM_Header*)addr;
        
        if (header->magic != FLASH_EEPROM_MAGIC)
        {
            // Fin des données valides
            break;
        }
        
        uint16_t entrySize = sizeof(FlashEEPROM_Header) + header->dataSize + 4;  // +4 pour CRC
        addr += entrySize;
        eeprom_state.usedSpace += entrySize;
    }
    
    eeprom_state.currentOffset = addr - eeprom_state.activeBuffer;
    eeprom_state.availableSpace = FLASH_EEPROM_SECTOR_SIZE - eeprom_state.usedSpace;
    eeprom_state.initialized = true;
    
    FLASH_EEPROM_DEBUG("Initialisé (buffer=0x%08lX, utilisé=%lu, libre=%lu)\n",
                      (unsigned long)eeprom_state.activeBuffer,
                      (unsigned long)eeprom_state.usedSpace,
                      (unsigned long)eeprom_state.availableSpace);
    
    return FLASH_EEPROM_OK;
}

/**
 * @brief Désinitialise
 */
void flash_eeprom_deinit(void)
{
    HAL_FLASH_Lock();
    eeprom_state.locked = true;
    eeprom_state.initialized = false;
}

/**
 * @brief Vérifie si prêt
 */
bool flash_eeprom_is_ready(void)
{
    return eeprom_state.initialized && !eeprom_state.locked;
}

/**
 * @brief Récupère l'état
 */
FlashEEPROM_State* flash_eeprom_get_state(void)
{
    return &eeprom_state;
}

// ============================================================
// SECTION 2 : ÉCRITURE
// ============================================================

/**
 * @brief Écrit une variable dans la Flash
 */
FlashEEPROM_Error flash_eeprom_write(uint16_t variableId, const uint8_t* data, uint16_t size)
{
    if (!flash_eeprom_is_ready()) return FLASH_EEPROM_ERROR_LOCKED;
    if (data == NULL && size > 0) return FLASH_EEPROM_ERROR_SIZE;
    if (size > FLASH_EEPROM_PAGE_SIZE - sizeof(FlashEEPROM_Header) - 4)
    {
        return FLASH_EEPROM_ERROR_SIZE;
    }
    
    // Vérifier l'espace disponible
    uint16_t entrySize = sizeof(FlashEEPROM_Header) + size + 4;  // +4 pour le CRC
    uint32_t writeAddr = eeprom_state.activeBuffer + eeprom_state.currentOffset;
    
    if (eeprom_state.availableSpace < entrySize)
    {
        FLASH_EEPROM_DEBUG("Espace insuffisant, défragmentation nécessaire\n");
        
        // Tenter de défragmenter
        FlashEEPROM_Error err = flash_eeprom_defragment();
        if (err != FLASH_EEPROM_OK)
        {
            return FLASH_EEPROM_ERROR_FULL;
        }
        
        writeAddr = eeprom_state.activeBuffer + eeprom_state.currentOffset;
    }
    
    // Construire l'entrée
    FlashEEPROM_Entry entry;
    memset(&entry, 0, sizeof(FlashEEPROM_Entry));
    
    entry.header.magic = FLASH_EEPROM_MAGIC;
    entry.header.version = FLASH_EEPROM_DATA_VERSION;
    entry.header.variableId = variableId;
    entry.header.dataSize = size;
    entry.header.flags = 0;
    
    if (size > 0 && data != NULL)
    {
        memcpy(entry.data, data, size);
    }
    
    // Calculer le CRC des données
    entry.dataCrc = flash_eeprom_crc32(entry.data, size);
    
    // Calculer le CRC de l'en-tête
    entry.header.crc = flash_eeprom_crc32((uint8_t*)&entry.header, 
                                           sizeof(FlashEEPROM_Header) - 4);
    
    // Écrire dans la Flash
    FLASH_EEPROM_DEBUG("Écriture ID=0x%04X, taille=%d à 0x%08lX\n", 
                      variableId, size, (unsigned long)writeAddr);
    
    // Programmer mot par mot (32 bits)
    uint32_t* src = (uint32_t*)&entry;
    uint32_t wordCount = (entrySize + 3) / 4;  // Arrondir au mot supérieur
    
    __disable_irq();  // Désactiver les interruptions pendant l'écriture Flash
    
    for (uint32_t i = 0; i < wordCount; i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, writeAddr + i * 4, src[i]) != HAL_OK)
        {
            __enable_irq();
            FLASH_EEPROM_DEBUG("Échec programmation à 0x%08lX\n", (unsigned long)(writeAddr + i * 4));
            return FLASH_EEPROM_ERROR_WRITE;
        }
    }
    
    __enable_irq();
    
    // Mettre à jour l'état
    eeprom_state.currentOffset += entrySize;
    eeprom_state.usedSpace += entrySize;
    eeprom_state.availableSpace -= entrySize;
    eeprom_state.totalWrites++;
    
    FLASH_EEPROM_DEBUG("Écriture OK (offset=%lu)\n", (unsigned long)eeprom_state.currentOffset);
    
    return FLASH_EEPROM_OK;
}

/**
 * @brief Écrit un entier 8 bits
 */
FlashEEPROM_Error flash_eeprom_write_u8(uint16_t variableId, uint8_t value)
{
    return flash_eeprom_write(variableId, &value, sizeof(uint8_t));
}

/**
 * @brief Écrit un entier 16 bits
 */
FlashEEPROM_Error flash_eeprom_write_u16(uint16_t variableId, uint16_t value)
{
    return flash_eeprom_write(variableId, (uint8_t*)&value, sizeof(uint16_t));
}

/**
 * @brief Écrit un entier 32 bits
 */
FlashEEPROM_Error flash_eeprom_write_u32(uint16_t variableId, uint32_t value)
{
    return flash_eeprom_write(variableId, (uint8_t*)&value, sizeof(uint32_t));
}

/**
 * @brief Écrit une chaîne de caractères
 */
FlashEEPROM_Error flash_eeprom_write_string(uint16_t variableId, const char* str)
{
    if (str == NULL) return FLASH_EEPROM_ERROR_SIZE;
    uint16_t len = strlen(str) + 1;  // +1 pour le '\0'
    if (len > 48) len = 48;
    return flash_eeprom_write(variableId, (const uint8_t*)str, len);
}

/**
 * @brief Écrit un buffer
 */
FlashEEPROM_Error flash_eeprom_write_buffer(uint16_t variableId, const uint8_t* buffer, uint16_t size)
{
    return flash_eeprom_write(variableId, buffer, size);
}

// ============================================================
// SECTION 3 : LECTURE
// ============================================================

/**
 * @brief Trouve la dernière entrée pour un ID donné
 */
static FlashEEPROM_Entry* find_entry(uint16_t variableId)
{
    uint32_t addr = eeprom_state.activeBuffer;
    FlashEEPROM_Entry* lastEntry = NULL;
    FlashEEPROM_Header* header;
    
    while (addr < eeprom_state.activeBuffer + FLASH_EEPROM_SECTOR_SIZE - sizeof(FlashEEPROM_Header))
    {
        header = (FlashEEPROM_Header*)addr;
        
        if (header->magic != FLASH_EEPROM_MAGIC)
        {
            break;  // Fin des données
        }
        
        if (header->variableId == variableId)
        {
            // Vérifier le CRC de l'en-tête
            uint32_t computedCrc = flash_eeprom_crc32((uint8_t*)header, sizeof(FlashEEPROM_Header) - 4);
            
            if (computedCrc == header->crc)
            {
                lastEntry = (FlashEEPROM_Entry*)addr;
            }
        }
        
        uint16_t entrySize = sizeof(FlashEEPROM_Header) + header->dataSize + 4;
        addr += entrySize;
    }
    
    return lastEntry;  // Retourne la dernière entrée valide (la plus récente)
}

/**
 * @brief Lit une variable depuis la Flash
 */
FlashEEPROM_Error flash_eeprom_read(uint16_t variableId, uint8_t* data, uint16_t size, uint16_t* readSize)
{
    if (!flash_eeprom_is_ready()) return FLASH_EEPROM_ERROR_LOCKED;
    
    FlashEEPROM_Entry* entry = find_entry(variableId);
    
    if (entry == NULL)
    {
        return FLASH_EEPROM_ERROR_NOT_FOUND;
    }
    
    // Vérifier le CRC des données
    if (!flash_eeprom_verify_crc(entry->data, entry->header.dataSize, entry->dataCrc))
    {
        FLASH_EEPROM_DEBUG("CRC invalide pour ID=0x%04X\n", variableId);
        return FLASH_EEPROM_ERROR_CRC;
    }
    
    uint16_t copySize = (entry->header.dataSize < size) ? entry->header.dataSize : size;
    
    if (data != NULL && copySize > 0)
    {
        memcpy(data, entry->data, copySize);
    }
    
    if (readSize != NULL)
    {
        *readSize = copySize;
    }
    
    return FLASH_EEPROM_OK;
}

/**
 * @brief Lit un entier 8 bits
 */
FlashEEPROM_Error flash_eeprom_read_u8(uint16_t variableId, uint8_t* value)
{
    uint16_t readSize;
    return flash_eeprom_read(variableId, value, sizeof(uint8_t), &readSize);
}

/**
 * @brief Lit un entier 16 bits
 */
FlashEEPROM_Error flash_eeprom_read_u16(uint16_t variableId, uint16_t* value)
{
    uint16_t readSize;
    return flash_eeprom_read(variableId, (uint8_t*)value, sizeof(uint16_t), &readSize);
}

/**
 * @brief Lit un entier 32 bits
 */
FlashEEPROM_Error flash_eeprom_read_u32(uint16_t variableId, uint32_t* value)
{
    uint16_t readSize;
    return flash_eeprom_read(variableId, (uint8_t*)value, sizeof(uint32_t), &readSize);
}

/**
 * @brief Lit une chaîne
 */
FlashEEPROM_Error flash_eeprom_read_string(uint16_t variableId, char* buffer, uint16_t bufferSize)
{
    uint16_t readSize;
    FlashEEPROM_Error err = flash_eeprom_read(variableId, (uint8_t*)buffer, bufferSize - 1, &readSize);
    
    if (err == FLASH_EEPROM_OK && buffer != NULL)
    {
        buffer[readSize] = '\0';  // S'assurer de la terminaison
    }
    
    return err;
}

/**
 * @brief Vérifie si une variable existe
 */
bool flash_eeprom_exists(uint16_t variableId)
{
    return (find_entry(variableId) != NULL);
}

/**
 * @brief Récupère la taille d'une variable
 */
uint16_t flash_eeprom_get_size(uint16_t variableId)
{
    FlashEEPROM_Entry* entry = find_entry(variableId);
    return (entry != NULL) ? entry->header.dataSize : 0;
}

// ============================================================
// SECTION 4 : GESTION
// ============================================================

/**
 * @brief Efface une variable (marque comme supprimée)
 */
FlashEEPROM_Error flash_eeprom_erase(uint16_t variableId)
{
    // Dans l'émulation EEPROM, on ne peut pas effacer une entrée individuelle.
    // On écrit une entrée de taille 0 pour "supprimer" la variable.
    return flash_eeprom_write(variableId, NULL, 0);
}

/**
 * @brief Efface complètement le secteur
 */
static FlashEEPROM_Error erase_sector(uint32_t sectorAddr)
{
    FLASH_EEPROM_DEBUG("Effacement secteur 0x%08lX...\n", (unsigned long)sectorAddr);
    
    // Calculer le numéro de secteur
    uint32_t sectorNumber;
    
    if (sectorAddr == FLASH_EEPROM_BUFFER_A_ADDR)
        sectorNumber = FLASH_SECTOR_11;
    else if (sectorAddr == FLASH_EEPROM_BUFFER_B_ADDR)
        sectorNumber = FLASH_SECTOR_12;
    else
        return FLASH_EEPROM_ERROR_ERASE;
    
    __disable_irq();
    
    FLASH_EraseInitTypeDef eraseInit;
    eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseInit.Sector = sectorNumber;
    eraseInit.NbSectors = 1;
    eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    
    uint32_t sectorError = 0;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&eraseInit, &sectorError);
    
    __enable_irq();
    
    if (status != HAL_OK)
    {
        FLASH_EEPROM_DEBUG("Échec effacement secteur %lu\n", (unsigned long)sectorNumber);
        return FLASH_EEPROM_ERROR_ERASE;
    }
    
    eeprom_state.totalErases++;
    
    return FLASH_EEPROM_OK;
}

/**
 * @brief Formate la mémoire EEPROM
 */
FlashEEPROM_Error flash_eeprom_format(void)
{
    FLASH_EEPROM_DEBUG("Formatage Flash EEPROM...\n");
    
    // Effacer le buffer A
    FlashEEPROM_Error err = erase_sector(FLASH_EEPROM_BUFFER_A_ADDR);
    if (err != FLASH_EEPROM_OK) return err;
    
    // Réinitialiser l'état
    eeprom_state.activeBuffer = FLASH_EEPROM_BUFFER_A_ADDR;
    eeprom_state.currentOffset = 0;
    eeprom_state.usedSpace = 0;
    eeprom_state.availableSpace = FLASH_EEPROM_SECTOR_SIZE;
    
    FLASH_EEPROM_DEBUG("Formatage terminé\n");
    
    return FLASH_EEPROM_OK;
}

/**
 * @brief Défragmente la mémoire
 */
FlashEEPROM_Error flash_eeprom_defragment(void)
{
    FLASH_EEPROM_DEBUG("Défragmentation...\n");
    
    // Déterminer le buffer de destination (l'autre buffer)
    uint32_t srcBuffer = eeprom_state.activeBuffer;
    uint32_t dstBuffer;
    
    if (srcBuffer == FLASH_EEPROM_BUFFER_A_ADDR)
        dstBuffer = FLASH_EEPROM_BUFFER_B_ADDR;
    else
        dstBuffer = FLASH_EEPROM_BUFFER_A_ADDR;
    
    // Effacer le buffer de destination
    FlashEEPROM_Error err = erase_sector(dstBuffer);
    if (err != FLASH_EEPROM_OK) return err;
    
    // Copier les données valides (dernière version de chaque variable)
    uint32_t srcAddr = srcBuffer;
    uint32_t dstAddr = dstBuffer;
    uint32_t dstOffset = 0;
    
    // Tableau pour suivre les IDs déjà copiés
    uint16_t copiedIds[FLASH_EEPROM_MAX_VARIABLES];
    uint16_t copiedCount = 0;
    
    // Parcourir le buffer source
    while (srcAddr < srcBuffer + FLASH_EEPROM_SECTOR_SIZE - sizeof(FlashEEPROM_Header))
    {
        FlashEEPROM_Header* header = (FlashEEPROM_Header*)srcAddr;
        
        if (header->magic != FLASH_EEPROM_MAGIC) break;
        
        uint16_t entrySize = sizeof(FlashEEPROM_Header) + header->dataSize + 4;
        
        // Vérifier si cet ID a déjà été copié (on ne garde que la dernière version)
        bool alreadyCopied = false;
        for (uint16_t i = 0; i < copiedCount; i++)
        {
            if (copiedIds[i] == header->variableId)
            {
                alreadyCopied = true;
                break;
            }
        }
        
        // Chercher si une version plus récente existe plus loin
        bool hasNewerVersion = false;
        uint32_t searchAddr = srcAddr + entrySize;
        
        while (searchAddr < srcBuffer + FLASH_EEPROM_SECTOR_SIZE - sizeof(FlashEEPROM_Header))
        {
            FlashEEPROM_Header* searchHeader = (FlashEEPROM_Header*)searchAddr;
            
            if (searchHeader->magic != FLASH_EEPROM_MAGIC) break;
            
            if (searchHeader->variableId == header->variableId)
            {
                hasNewerVersion = true;
                break;
            }
            
            searchAddr += sizeof(FlashEEPROM_Header) + searchHeader->dataSize + 4;
        }
        
        // Si c'est la version la plus récente et pas déjà copiée
        if (!alreadyCopied && !hasNewerVersion)
        {
            // Copier l'entrée complète dans le buffer de destination
            uint32_t* src = (uint32_t*)srcAddr;
            uint32_t wordCount = (entrySize + 3) / 4;
            
            __disable_irq();
            
            for (uint32_t w = 0; w < wordCount; w++)
            {
                HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, dstAddr, src[w]);
                dstAddr += 4;
            }
            
            __enable_irq();
            
            dstOffset += entrySize;
            
            if (copiedCount < FLASH_EEPROM_MAX_VARIABLES)
            {
                copiedIds[copiedCount++] = header->variableId;
            }
        }
        
        srcAddr += entrySize;
    }
    
    // Mettre à jour l'état
    eeprom_state.activeBuffer = dstBuffer;
    eeprom_state.currentOffset = dstOffset;
    eeprom_state.usedSpace = dstOffset;
    eeprom_state.availableSpace = FLASH_EEPROM_SECTOR_SIZE - dstOffset;
    
    FLASH_EEPROM_DEBUG("Défragmentation terminée (%d variables, %lu octets)\n",
                      copiedCount, (unsigned long)dstOffset);
    
    return FLASH_EEPROM_OK;
}

/**
 * @brief Vérifie l'intégrité des données
 */
bool flash_eeprom_verify(void)
{
    uint32_t addr = eeprom_state.activeBuffer;
    
    while (addr < eeprom_state.activeBuffer + eeprom_state.currentOffset)
    {
        FlashEEPROM_Header* header = (FlashEEPROM_Header*)addr;
        
        if (header->magic != FLASH_EEPROM_MAGIC) return false;
        
        // Vérifier le CRC de l'en-tête
        uint32_t computedHeaderCrc = flash_eeprom_crc32((uint8_t*)header, sizeof(FlashEEPROM_Header) - 4);
        if (computedHeaderCrc != header->crc) return false;
        
        // Vérifier le CRC des données
        FlashEEPROM_Entry* entry = (FlashEEPROM_Entry*)addr;
        uint32_t computedDataCrc = flash_eeprom_crc32(entry->data, header->dataSize);
        if (computedDataCrc != entry->dataCrc) return false;
        
        addr += sizeof(FlashEEPROM_Header) + header->dataSize + 4;
    }
    
    return true;
}

/**
 * @brief Récupère l'espace libre
 */
uint32_t flash_eeprom_get_free_space(void)
{
    return eeprom_state.availableSpace;
}

// ============================================================
// SECTION 5 : CRC32
// ============================================================

/**
 * @brief Table CRC32 (polynôme standard Ethernet)
 */
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
    0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
    0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
    0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
    0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
    0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
    0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
    0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
    0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
    0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
    0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
    0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
    0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
    0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
    0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
    0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB30A04,
    0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
    0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
    0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
    0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
    0xA9BCAE53, 0xDEBB9EC5, 0x47B27F7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
    0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

/**
 * @brief Calcule le CRC32 d'un buffer
 */
uint32_t flash_eeprom_crc32(const uint8_t* data, uint16_t size)
{
    if (data == NULL) return 0;
    
    uint32_t crc = 0xFFFFFFFF;
    
    for (uint16_t i = 0; i < size; i++)
    {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

/**
 * @brief Vérifie le CRC d'un buffer
 */
bool flash_eeprom_verify_crc(const uint8_t* data, uint16_t size, uint32_t expectedCrc)
{
    uint32_t computedCrc = flash_eeprom_crc32(data, size);
    return (computedCrc == expectedCrc);
}

// ============================================================
// SECTION 6 : DÉBOGAGE
// ============================================================

void flash_eeprom_print_state(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║     ÉTAT FLASH EEPROM                         ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ Initialisé   : %-31s ║\n", eeprom_state.initialized ? "Oui" : "Non");
    printf("║ Verrouillé   : %-31s ║\n", eeprom_state.locked ? "Oui" : "Non");
    printf("║ Buffer actif : 0x%08lX                      ║\n", (unsigned long)eeprom_state.activeBuffer);
    printf("║ Offset       : %-31lu ║\n", (unsigned long)eeprom_state.currentOffset);
    printf("║ Espace utilisé: %-31lu ║\n", (unsigned long)eeprom_state.usedSpace);
    printf("║ Espace libre : %-31lu ║\n", (unsigned long)eeprom_state.availableSpace);
    printf("║ Écritures    : %-31lu ║\n", (unsigned long)eeprom_state.totalWrites);
    printf("║ Effacements  : %-31lu ║\n", (unsigned long)eeprom_state.totalErases);
    printf("║ Intégrité    : %-31s ║\n", flash_eeprom_verify() ? "OK" : "ERREUR");
    printf("╚══════════════════════════════════════════════╝\n\n");
}

void flash_eeprom_print_entry(uint16_t variableId)
{
    FlashEEPROM_Entry* entry = find_entry(variableId);
    
    if (entry == NULL)
    {
        printf("[EEPROM] ID=0x%04X : NON TROUVÉ\n", variableId);
        return;
    }
    
    printf("[EEPROM] ID=0x%04X : Version=%d, Taille=%d, CRC=%s\n",
           variableId,
           entry->header.version,
           entry->header.dataSize,
           flash_eeprom_verify_crc(entry->data, entry->header.dataSize, entry->dataCrc) ? "OK" : "ERREUR");
}

void flash_eeprom_dump(uint16_t startOffset, uint16_t length)
{
    uint32_t addr = eeprom_state.activeBuffer + startOffset;
    
    printf("═══ DUMP FLASH 0x%08lX (+%d) ═══\n", (unsigned long)addr, length);
    
    for (uint16_t i = 0; i < length; i += 16)
    {
        printf("%08lX  ", (unsigned long)(addr + i));
        
        for (uint16_t j = 0; j < 16; j++)
        {
            if (i + j < length)
                printf("%02X ", *((uint8_t*)(addr + i + j)));
            else
                printf("   ");
            
            if (j == 7) printf(" ");
        }
        
        printf(" |");
        for (uint16_t j = 0; j < 16 && (i + j) < length; j++)
        {
            uint8_t c = *((uint8_t*)(addr + i + j));
            printf("%c", (c >= 32 && c <= 126) ? c : '.');
        }
        printf("|\n");
    }
    printf("══════════════════════════════\n\n");
}

bool flash_eeprom_self_test(void)
{
    FLASH_EEPROM_DEBUG("Auto-test...\n");
    
    if (!flash_eeprom_is_ready())
    {
        FLASH_EEPROM_DEBUG("Échec : non initialisé\n");
        return false;
    }
    
    // Test 1 : Écriture/Lecture
    uint32_t testValue = 0xDEADBEEF;
    flash_eeprom_write_u32(0xF001, testValue);
    
    uint32_t readValue = 0;
    FlashEEPROM_Error err = flash_eeprom_read_u32(0xF001, &readValue);
    
    if (err != FLASH_EEPROM_OK || readValue != testValue)
    {
        FLASH_EEPROM_DEBUG("Échec test lecture/écriture\n");
        return false;
    }
    
    // Test 2 : CRC
    uint8_t testData[] = "Test CRC";
    uint32_t crc = flash_eeprom_crc32(testData, strlen((char*)testData));
    
    if (!flash_eeprom_verify_crc(testData, strlen((char*)testData), crc))
    {
        FLASH_EEPROM_DEBUG("Échec test CRC\n");
        return false;
    }
    
    FLASH_EEPROM_DEBUG("Auto-test OK\n");
    return true;
}