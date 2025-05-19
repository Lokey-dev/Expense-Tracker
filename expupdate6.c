#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sqlite3.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

#define DATE_LEN 11
#define TYPE_LEN 30

typedef struct Expense {
    int id;
    char date[DATE_LEN];
    char type[TYPE_LEN];
    double amount;
    struct Expense* next;
} Expense;

Expense* head = NULL;
double budget = 0.0;
sqlite3* db;

// Date validation helpers
int isLeapYear(int year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

int isValidDate(int day, int month, int year) {
    if (year < 1 || year > 9999 || month < 1 || month > 12)
        return 0;

    int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (isLeapYear(year)) daysInMonth[1] = 29;

    return day >= 1 && day <= daysInMonth[month - 1];
}

int isDateFormatValid(const char* date) {
    if (strlen(date) != 10 || date[2] != '/' || date[5] != '/')
        return 0;
    for (int i = 0; i < 10; i++) {
        if (i == 2 || i == 5) continue;
        if (!isdigit(date[i])) return 0;
    }
    return 1;
}

int validateDateInput(const char* dateStr) {
    if (!isDateFormatValid(dateStr)) return 0;
    int day = atoi(dateStr);
    int month = atoi(dateStr + 3);
    int year = atoi(dateStr + 6);
    return isValidDate(day, month, year);
}

// Function declarations
void initDB();
void loadExpensesFromDB();
void saveExpenseToDB(Expense*);
void updateExpenseInDB(Expense*);
void deleteExpenseFromDB(int);
void exportToCSV();
void importFromCSV();
void setBudget();
void addExpense();
void editExpense();
void deleteExpense();
void viewExpenses();
void viewExpensesByCategory();
double viewTotal();
double getRemainingBudget();
Expense* createExpenseWithID(int, const char*, const char*, double);
Expense* createExpense(const char*, const char*, double);
void appendExpense(Expense*);
void freeExpenses();

void initDB() {
    if (sqlite3_open("expensesfinal.db", &db)) {
        printf("Can't open database: %s\n", sqlite3_errmsg(db));
        exit(1);
    }

    char* errMsg = 0;
    const char* sql = "CREATE TABLE IF NOT EXISTS expenses ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "date TEXT, type TEXT, amount REAL);";
    if (sqlite3_exec(db, sql, 0, 0, &errMsg) != SQLITE_OK) {
        printf("SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
    }
}

Expense* createExpenseWithID(int id, const char* date, const char* type, double amount) {
    Expense* newExp = (Expense*)malloc(sizeof(Expense));
    newExp->id = id;
    strcpy(newExp->date, date);
    strcpy(newExp->type, type);
    newExp->amount = amount;
    newExp->next = NULL;
    return newExp;
}

Expense* createExpense(const char* date, const char* type, double amount) {
    return createExpenseWithID(0, date, type, amount);
}

void appendExpense(Expense* newExp) {
    if (!head) {
        head = newExp;
    } else {
        Expense* temp = head;
        while (temp->next) temp = temp->next;
        temp->next = newExp;
    }
}

void saveExpenseToDB(Expense* e) {
    char sql[256];
    snprintf(sql, sizeof(sql),
             "INSERT INTO expenses (date, type, amount) VALUES ('%s', '%s', %f);",
             e->date, e->type, e->amount);
    char* errMsg = 0;
    if (sqlite3_exec(db, sql, 0, 0, &errMsg) != SQLITE_OK) {
        printf("Failed to insert: %s\n", errMsg);
        sqlite3_free(errMsg);
    }
}

void updateExpenseInDB(Expense* e) {
    char sql[256];
    snprintf(sql, sizeof(sql),
             "UPDATE expenses SET date='%s', type='%s', amount=%f WHERE id=%d;",
             e->date, e->type, e->amount, e->id);
    char* errMsg = 0;
    if (sqlite3_exec(db, sql, 0, 0, &errMsg) != SQLITE_OK) {
        printf("Failed to update: %s\n", errMsg);
        sqlite3_free(errMsg);
    }
}

void addExpense() {
    if (viewTotal() >= budget && budget > 0) {
        printf("WARNING: You have exceeded your budget! Set a new budget first.\n");
        return;
    }

    char date[DATE_LEN], type[TYPE_LEN];
    double amount;

    do {
        printf("Enter date (dd/mm/yyyy): ");
        scanf("%s", date);
        if (!validateDateInput(date))
            printf("Invalid date format or value. Try again.\n");
    } while (!validateDateInput(date));

    printf("Enter type of expense: ");
    scanf("%s", type);

    do {
        printf("Enter amount: ");
        scanf("%lf", &amount);
        if (amount < 0) printf("Amount cannot be negative.\n");
    } while (amount < 0);

    if ((viewTotal() + amount) > budget && budget > 0) {
        printf("WARNING: This expense exceeds your budget!\n");
        return;
    }

    Expense* exp = createExpense(date, type, amount);
    appendExpense(exp);
    saveExpenseToDB(exp);
    printf("Expense added. Remaining Budget: Rs%.2f\n", getRemainingBudget());
}

void deleteExpenseFromDB(int id) {
    char sql[128];
    snprintf(sql, sizeof(sql), "DELETE FROM expenses WHERE id=%d;", id);
    char* errMsg = 0;
    if (sqlite3_exec(db, sql, 0, 0, &errMsg) != SQLITE_OK) {
        printf("Failed to delete: %s\n", errMsg);
        sqlite3_free(errMsg);
    }
}

void loadExpensesFromDB() {
    freeExpenses();
    const char* sql = "SELECT id, date, type, amount FROM expenses;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char* date = (const char*)sqlite3_column_text(stmt, 1);
        const char* type = (const char*)sqlite3_column_text(stmt, 2);
        double amount = sqlite3_column_double(stmt, 3);

        Expense* e = createExpenseWithID(id, date, type, amount);
        appendExpense(e);
    }

    sqlite3_finalize(stmt);
}

void setBudget() {
    printf("Enter your budget: Rs");
    scanf("%lf", &budget);
    printf("Budget set to Rs%.2f\n", budget);
}

double viewTotal() {
    double total = 0.0;
    Expense* temp = head;
    while (temp) {
        total += temp->amount;
        temp = temp->next;
    }
    printf("Total Expense: Rs%.2f\n", total);
    return total;
}

double getRemainingBudget() {
    return budget - viewTotal();
}

void viewExpenses() {
    if (!head) {
        printf("No expenses recorded.\n");
        return;
    }
    printf("%-5s %-15s %-15s %-10s\n", "S.No", "Date", "Type", "Amount");
    int i = 1;
    Expense* temp = head;
    while (temp) {
        printf("%-5d %-15s %-15s Rs%.2f\n", i++, temp->date, temp->type, temp->amount);
        temp = temp->next;
    }
}

void viewExpensesByCategory() {
    if (!head) {
        printf("No expenses recorded.\n");
        return;
    }
    char searchType[TYPE_LEN];
    printf("Enter the category to search for: ");
    scanf("%s", searchType);
    Expense* temp = head;
    int found = 0, i = 1;
    printf("%-5s %-15s %-15s %-10s\n", "S.No", "Date", "Type", "Amount");
    while (temp) {
        if (strcasecmp(temp->type, searchType) == 0) {
            printf("%-5d %-15s %-15s Rs%.2f\n", i, temp->date, temp->type, temp->amount);
            found = 1;
        }
        temp = temp->next;
        i++;
    }
    if (!found) {
        printf("No expenses found for category '%s'.\n", searchType);
    }
}

void editExpense() {
    viewExpenses();
    int index;
    printf("Enter S.No. to edit: ");
    scanf("%d", &index);
    Expense* temp = head;
    for (int i = 1; i < index && temp; i++) temp = temp->next;
    if (!temp) {
        printf("Invalid S.No.\n");
        return;
    }

    do {
        printf("Enter new date (dd/mm/yyyy): ");
        scanf("%s", temp->date);
        if (!validateDateInput(temp->date))
            printf("Invalid date format or value. Try again.\n");
    } while (!validateDateInput(temp->date));

    printf("Enter new type: ");
    scanf("%s", temp->type);
    do {
        printf("Enter new amount: ");
        scanf("%lf", &temp->amount);
        if (temp->amount < 0) printf("Amount cannot be negative.\n");
    } while (temp->amount < 0);
    updateExpenseInDB(temp);
    printf("Expense updated successfully.\n");
}

void deleteExpense() {
    viewExpenses();
    int index;
    printf("Enter S.No. to delete: ");
    scanf("%d", &index);
    if (index < 1 || !head) {
        printf("Invalid S.No.\n");
        return;
    }
    Expense* temp = head;
    if (index == 1) {
        head = head->next;
        deleteExpenseFromDB(temp->id);
        free(temp);
    } else {
        Expense* prev = NULL;
        for (int i = 1; i < index && temp; i++) {
            prev = temp;
            temp = temp->next;
        }
        if (!temp) {
            printf("Invalid S.No.\n");
            return;
        }
        prev->next = temp->next;
        deleteExpenseFromDB(temp->id);
        free(temp);
    }
    printf("Expense deleted. Remaining Budget: Rs%.2f\n", getRemainingBudget());
}

void freeExpenses() {
    Expense* temp;
    while (head) {
        temp = head;
        head = head->next;
        free(temp);
    }
}

void exportToCSV() {
    FILE* file = fopen("expensesfinal.csv", "w");
    if (!file) {
        printf("Failed to create CSV file.\n");
        return;
    }

    fprintf(file, "S.No,Date,Type,Amount\n");

    Expense* temp = head;
    int sno = 1;
    while (temp) {
        // Defensive checks to ensure valid strings
        if (temp->date[0] != '\0' && temp->type[0] != '\0') {
            fprintf(file, "%d,%s,%s,%.2f\n", sno++, temp->date, temp->type, temp->amount);
        } else {
            printf("Skipping invalid expense entry with empty fields.\n");
        }
        temp = temp->next;
    }

    fclose(file);
    printf("Expenses exported to expensesfinal.csv\n");
}


void importFromCSV() {
    FILE* file = fopen("expensesfinal.csv", "r");
    if (!file) {
        printf("Could not open expensesfinal.csv for import.\n");
        return;
    }

    char sqlClear[] = "DELETE FROM expenses;";
    char* errMsg = 0;
    if (sqlite3_exec(db, sqlClear, 0, 0, &errMsg) != SQLITE_OK) {
        printf("Failed to clear old records: %s\n", errMsg);
        sqlite3_free(errMsg);
    }
    freeExpenses();

    char line[256];
    fgets(line, sizeof(line), file); // Skip header line

    while (fgets(line, sizeof(line), file)) {
        // Remove newline character at end
        line[strcspn(line, "\r\n")] = 0;

        char date[DATE_LEN], type[TYPE_LEN];
        double amount;
        int sno;

        if (sscanf(line, "%d,%10[^,],%29[^,],%lf", &sno, date, type, &amount) == 4) {
            Expense* e = createExpense(date, type, amount);
            appendExpense(e);
            saveExpenseToDB(e);
        } else {
            printf("Skipping invalid line: %s\n", line); // Debug line
        }
    }

    fclose(file);
    printf("Expenses imported from expensesfinal.csv\n");
}


int main() {
    initDB();
    loadExpensesFromDB();
    int choice;
    while (1) {
        printf("\n==== Expense Tracker ====\n");
        printf("1. Set Budget\n");
        printf("2. Add Expense\n");
        printf("3. Edit Expense\n");
        printf("4. Delete Expense\n");
        printf("5. View All Expenses\n");
        printf("6. View Expenses by Category\n");
        printf("7. View Total\n");
        printf("8. Export to CSV\n");
        printf("9. Import from CSV\n");
        printf("10. Exit\n");
        printf("=========================\n");
        printf("Choose an option: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1: setBudget(); break;
            case 2: addExpense(); break;
            case 3: editExpense(); break;
            case 4: deleteExpense(); break;
            case 5: viewExpenses(); break;
            case 6: viewExpensesByCategory(); break;
            case 7: viewTotal(); break;
            case 8: exportToCSV(); break;
            case 9: importFromCSV(); break;
            case 10:
                freeExpenses();
                sqlite3_close(db);
                exit(0);
            default: printf("Invalid option.\n");
        }
    }
}
