#pragma once

#include "PDCEL.hpp"
#include "PDCELValidator.hpp"
#include "plog.hpp"

#include <functional>
#include <vector>
#include <memory>

/**
 * @class PDCELTransaction
 * @brief RAII-style transaction manager for PDCEL operations
 * 
 * This class provides a way to perform multiple DCEL operations as a single transaction.
 * If any operation fails, all changes are rolled back. The transaction is automatically
 * committed when the object is destroyed, unless explicitly rolled back.
 */
class PDCELTransaction {
public:
    /**
     * @brief Construct a new transaction
     * @param dcel The DCEL to manage
     */
    explicit PDCELTransaction(PDCEL* dcel);

    /**
     * @brief Destructor - automatically commits if not rolled back
     */
    ~PDCELTransaction();

    /**
     * @brief Commit the transaction
     * @throws std::runtime_error if validation fails
     */
    void commit();

    /**
     * @brief Roll back the transaction
     */
    void rollback();

    /**
     * @brief Add a rollback action
     * @param action The action to perform during rollback
     */
    void addRollbackAction(std::function<void()> action);

    /**
     * @brief Get the managed DCEL
     * @return The DCEL being managed
     */
    PDCEL* getDCEL() const { return _dcel; }

    /**
     * @brief Check if the transaction is active
     * @return true if the transaction is active, false if committed or rolled back
     */
    bool isActive() const { return _active; }

private:
    PDCEL* _dcel;
    bool _active;
    std::vector<std::function<void()>> _rollback_actions;
    std::unique_ptr<PDCEL> _backup;

    /**
     * @brief Create a backup of the current DCEL state
     */
    void createBackup();

    /**
     * @brief Restore the DCEL from backup
     */
    void restoreFromBackup();

    /**
     * @brief Validate the DCEL state
     * @throws std::runtime_error if validation fails
     */
    void validateState();
}; 