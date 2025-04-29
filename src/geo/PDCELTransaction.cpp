#include "PDCELTransaction.hpp"
#include "PDCEL.hpp"
#include "PDCELValidator.hpp"
#include "plog.hpp"

#include <stdexcept>

PDCELTransaction::PDCELTransaction(PDCEL* dcel)
    : _dcel(dcel), _active(true) {
    PLOG(trace) << "Creating PDCEL transaction";
    
    if (!_dcel) {
        throw std::invalid_argument("DCEL cannot be null");
    }

    // Create backup of current state
    createBackup();
}

PDCELTransaction::~PDCELTransaction() {
    if (_active) {
        try {
            commit();
        } catch (const std::exception& e) {
            PLOG(error) << "Failed to commit transaction: " << e.what();
            rollback();
        }
    }
}

void PDCELTransaction::commit() {
    PLOG(trace) << "Committing PDCEL transaction";

    if (!_active) {
        throw std::runtime_error("Transaction is not active");
    }

    // Validate the final state
    validateState();

    // Clear rollback actions
    _rollback_actions.clear();
    _active = false;

    PLOG(trace) << "Transaction committed successfully";
}

void PDCELTransaction::rollback() {
    PLOG(trace) << "Rolling back PDCEL transaction";

    if (!_active) {
        PLOG(warning) << "Attempting to rollback inactive transaction";
        return;
    }

    // Execute rollback actions in reverse order
    for (auto it = _rollback_actions.rbegin(); it != _rollback_actions.rend(); ++it) {
        try {
            (*it)();
        } catch (const std::exception& e) {
            PLOG(error) << "Error during rollback: " << e.what();
        }
    }

    // Restore from backup
    restoreFromBackup();

    _rollback_actions.clear();
    _active = false;

    PLOG(trace) << "Transaction rolled back successfully";
}

void PDCELTransaction::addRollbackAction(std::function<void()> action) {
    if (!_active) {
        throw std::runtime_error("Cannot add rollback action to inactive transaction");
    }
    _rollback_actions.push_back(action);
}

void PDCELTransaction::createBackup() {
    PLOG(trace) << "Creating DCEL backup";

    // Create a deep copy of the DCEL
    _backup = std::make_unique<PDCEL>();
    
    // TODO: Implement deep copy of DCEL
    // This is a placeholder - actual implementation would need to copy all
    // vertices, edges, faces, and their relationships
    // For now, we'll just validate the current state
    validateState();
}

void PDCELTransaction::restoreFromBackup() {
    PLOG(trace) << "Restoring DCEL from backup";

    if (!_backup) {
        throw std::runtime_error("No backup available to restore from");
    }

    // TODO: Implement restoration from backup
    // This is a placeholder - actual implementation would need to restore all
    // vertices, edges, faces, and their relationships
    // For now, we'll just validate the current state
    validateState();
}

void PDCELTransaction::validateState() {
    PLOG(trace) << "Validating DCEL state";

    if (!PDCELValidator::validateDCEL(_dcel)) {
        std::string error = "DCEL validation failed: " + PDCELValidator::getLastError();
        PLOG(error) << error;
        throw std::runtime_error(error);
    }
}
