#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_ROWS   10
#define MAX_COLS   4
#define MAX_BUSES  5
#define NAME_LEN   50
#define ROUTE_LEN  40
#define TIME_LEN   10

#define REFUND_RATE      0.90f   
#define WINDOW_SURCHARGE 1.20f 

typedef struct Booking {
    int   id;
    char  name[NAME_LEN];
    int   row, col;
    float price;
    struct Booking *next;
} Booking;

typedef struct WaitNode {
    char  name[NAME_LEN];
    int   groupSize;
    int   advanceDays;
    struct WaitNode *next;
} WaitNode;

typedef struct {
    int      id;
    char     source[ROUTE_LEN];
    char     destination[ROUTE_LEN];
    char     departure[TIME_LEN];
    int      rows, cols;
    float    baseFare;
    int      seat[MAX_ROWS][MAX_COLS];  
    Booking  *bookings;                  
    WaitNode *wlHead, *wlTail;          
    int      booked;
    int      waiting;
    float    revenue;
} Bus;

static Bus buses[MAX_BUSES];
static int busCount = 0;
static int nextBookingId = 1001;

static void readLine(const char *prompt, char *buf, int size) {
    printf("%s", prompt);
    if (!fgets(buf, size, stdin)) { buf[0] = '\0'; return; }
    buf[strcspn(buf, "\n")] = '\0';
}

static int readInt(const char *prompt) {
    char buf[32], *end;
    long v;
    while (1) {
        readLine(prompt, buf, sizeof buf);
        v = strtol(buf, &end, 10);
        if (end != buf && *end == '\0') return (int)v;
        printf("  Invalid number, try again.\n");
    }
}

static void initBus(const char *src, const char *dst, const char *dep,
                    int rows, float fare) {
    if (busCount >= MAX_BUSES) return;
    Bus *b = &buses[busCount];
    memset(b, 0, sizeof *b);
    b->id = busCount + 1;
    strncpy(b->source, src, ROUTE_LEN - 1);
    strncpy(b->destination, dst, ROUTE_LEN - 1);
    strncpy(b->departure, dep, TIME_LEN - 1);
    b->rows = rows;
    b->cols = MAX_COLS;
    b->baseFare = fare;
    busCount++;
}

static Bus *getBus(int id) {
    if (id < 1 || id > busCount) return NULL;
    return &buses[id - 1];
}

static int totalSeats(const Bus *b) { return b->rows * b->cols; }
static int freeSeats(const Bus *b)  { return totalSeats(b) - b->booked; }
static int isWindow(const Bus *b, int col) { return col == 0 || col == b->cols - 1; }

static float calcPrice(const Bus *b, int col, int advanceDays) {
    float price = b->baseFare;
    if (isWindow(b, col)) price *= WINDOW_SURCHARGE;
    if      (advanceDays >= 15) price *= 0.80f;   /* 20% off */
    else if (advanceDays >= 7)  price *= 0.90f;   /* 10% off */
    else if (advanceDays >= 3)  price *= 0.95f;   /*  5% off */
    return price;
}

static void listBuses(void) {
    printf("\n%-4s %-14s %-14s %-8s %-6s %-8s %-6s\n",
           "ID", "From", "To", "Depart", "Free", "Fare", "Wait");
    printf("--------------------------------------------------------------\n");
    for (int i = 0; i < busCount; i++) {
        Bus *b = &buses[i];
        printf("%-4d %-14s %-14s %-8s %-6d Rs.%-5.0f %-6d\n", b->id, b->source,
               b->destination, b->departure, freeSeats(b), b->baseFare, b->waiting);
    }
}

static void displaySeatMap(const Bus *b) {
    printf("\nBus %d : %s -> %s  (%s)\n", b->id, b->source, b->destination, b->departure);
    printf("[ ] = free   [X] = booked   W = window seat\n\n");
    printf("        A    B   |   C    D\n");
    for (int r = 0; r < b->rows; r++) {
        printf("Row %-2d ", r + 1);
        for (int c = 0; c < b->cols; c++) {
            if (c == b->cols / 2) printf("  |  ");
            printf("[%c]%c ", b->seat[r][c] ? 'X' : ' ', isWindow(b, c) ? 'W' : ' ');
        }
        printf("\n");
    }
    printf("\nFree: %d / %d   Waitlist: %d\n", freeSeats(b), totalSeats(b), b->waiting);
}

static void searchBuses(void) {
    char src[ROUTE_LEN], dst[ROUTE_LEN];
    int found = 0;
    readLine("From (blank = any): ", src, sizeof src);
    readLine("To   (blank = any): ", dst, sizeof dst);
    printf("\nMatching buses:\n");
    for (int i = 0; i < busCount; i++) {
        Bus *b = &buses[i];
        /* case-insensitive substring match */
        char s1[ROUTE_LEN], s2[ROUTE_LEN], q1[ROUTE_LEN], q2[ROUTE_LEN];
        for (int k = 0; k < ROUTE_LEN; k++) {
            s1[k] = tolower((unsigned char)b->source[k]);
            s2[k] = tolower((unsigned char)b->destination[k]);
            q1[k] = tolower((unsigned char)src[k]);
            q2[k] = tolower((unsigned char)dst[k]);
        }
        if ((src[0] == '\0' || strstr(s1, q1)) && (dst[0] == '\0' || strstr(s2, q2))) {
            printf("  Bus %d: %s -> %s, departs %s, fare Rs.%.0f, free seats %d\n",
                   b->id, b->source, b->destination, b->departure, b->baseFare, freeSeats(b));
            found = 1;
        }
    }
    if (!found) printf("  No bus found.\n");
}

static Booking *addBooking(Bus *b, const char *name, int r, int c, int advanceDays) {
    Booking *nb = (Booking *)malloc(sizeof(Booking));
    if (!nb) { printf("Memory allocation failed!\n"); return NULL; }
    nb->id = nextBookingId++;
    strncpy(nb->name, name, NAME_LEN - 1);
    nb->name[NAME_LEN - 1] = '\0';
    nb->row = r;
    nb->col = c;
    nb->price = calcPrice(b, c, advanceDays);
    nb->next = NULL;

    /* append at tail */
    if (!b->bookings) b->bookings = nb;
    else {
        Booking *p = b->bookings;
        while (p->next) p = p->next;
        p->next = nb;
    }
    b->seat[r][c] = nb->id;
    b->booked++;
    b->revenue += nb->price;
    return nb;
}

static void printTicket(const Booking *bk) {
    printf("  Ticket #%d | %-15s | Seat %d%c (%s) | Rs.%.2f\n", bk->id, bk->name,
           bk->row + 1, 'A' + bk->col, (bk->col == 0 || bk->col == MAX_COLS - 1) ? "Window" : "Aisle",
           bk->price);
}

static void enqueueWait(Bus *b, const char *name, int groupSize, int advanceDays) {
    WaitNode *n = (WaitNode *)malloc(sizeof(WaitNode));
    if (!n) { printf("Memory allocation failed!\n"); return; }
    strncpy(n->name, name, NAME_LEN - 1);
    n->name[NAME_LEN - 1] = '\0';
    n->groupSize = groupSize;
    n->advanceDays = advanceDays;
    n->next = NULL;
    if (!b->wlTail) b->wlHead = b->wlTail = n;
    else { b->wlTail->next = n; b->wlTail = n; }
    b->waiting++;
}

static void showWaitlist(const Bus *b) {
    printf("\nWaitlist for Bus %d:\n", b->id);
    if (!b->wlHead) { printf("  (empty)\n"); return; }
    int pos = 1;
    for (WaitNode *w = b->wlHead; w; w = w->next)
        printf("  %d. %-15s  group of %d\n", pos++, w->name, w->groupSize);
}

static int findContiguous(const Bus *b, int n, int *outRow, int *outCol) {
    int bestScore = 1 << 30, found = 0;
    if (n > b->cols) return 0;
    for (int r = 0; r < b->rows; r++) {
        for (int c = 0; c + n <= b->cols; c++) {
            int ok = 1;
            for (int k = 0; k < n; k++)
                if (b->seat[r][c + k]) { ok = 0; break; }
            if (!ok) continue;
            int score = r;
            if (c < b->cols / 2 && c + n - 1 >= b->cols / 2) score += 3;
            if (score < bestScore) { bestScore = score; *outRow = r; *outCol = c; found = 1; }
        }
    }
    return found;
}

/* Dynamic allocation for a single passenger: pref 'W' window, 'A' aisle,
 * anything else = first free seat. Falls back to any seat if pref is full. */
static Booking *bookAuto(Bus *b, const char *name, char pref, int advanceDays) {
    pref = toupper((unsigned char)pref);
    for (int pass = 0; pass < 2; pass++) {
        for (int r = 0; r < b->rows; r++)
            for (int c = 0; c < b->cols; c++) {
                if (b->seat[r][c]) continue;
                int w = isWindow(b, c);
                if (pass == 0 && ((pref == 'W' && !w) || (pref == 'A' && w))) continue;
                return addBooking(b, name, r, c, advanceDays);
            }
    }
    return NULL;
}

/* Allocate a group of n passengers (assumes freeSeats >= n). */
static void allocateGroup(Bus *b, const char *name, int n, int advanceDays) {
    char pname[NAME_LEN];
    int r, c;
    if (findContiguous(b, n, &r, &c)) {
        printf("  Contiguous seats found in row %d.\n", r + 1);
        for (int k = 0; k < n; k++) {
            snprintf(pname, sizeof pname, "%s-%d", name, k + 1);
            Booking *bk = addBooking(b, pname, r, c + k, advanceDays);
            if (bk) printTicket(bk);
        }
    } else {
        printf("  No contiguous block of %d. Allocating nearest free seats.\n", n);
        for (int k = 0; k < n; k++) {
            snprintf(pname, sizeof pname, "%s-%d", name, k + 1);
            Booking *bk = bookAuto(b, pname, 'X', advanceDays);
            if (bk) printTicket(bk);
        }
    }
}

/* After a cancellation: promote waitlisted passengers/groups that now fit. */
static void processWaitlist(Bus *b) {
    WaitNode *prev = NULL, *cur = b->wlHead;
    while (cur) {
        if (freeSeats(b) >= cur->groupSize) {
            printf("\n  >> Waitlist promotion: %s (group of %d) gets seats on Bus %d\n",
                   cur->name, cur->groupSize, b->id);
            allocateGroup(b, cur->name, cur->groupSize, cur->advanceDays);

            WaitNode *dead = cur;
            if (prev) prev->next = cur->next; else b->wlHead = cur->next;
            if (b->wlTail == dead) b->wlTail = prev;
            cur = cur->next;
            free(dead);
            b->waiting--;
        } else {
            prev = cur;
            cur = cur->next;
        }
    }
}

static void bookSeatMenu(void) {
    Bus *b = getBus(readInt("Bus ID: "));
    if (!b) { printf("Invalid bus.\n"); return; }

    char name[NAME_LEN], buf[16];
    readLine("Passenger name: ", name, sizeof name);
    if (name[0] == '\0') { printf("Name required.\n"); return; }
    int adv = readInt("Days in advance (0 if today): ");
    if (adv < 0) adv = 0;

    if (freeSeats(b) == 0) {
        enqueueWait(b, name, 1, adv);
        printf("Bus is full. %s added to waitlist (position %d).\n", name, b->waiting);
        return;
    }

    readLine("Choose seat (e.g. 3B) or press Enter for auto: ", buf, sizeof buf);
    Booking *bk = NULL;
    if (buf[0] == '\0') {
        readLine("Preference - W(indow) / A(isle) / Enter = any: ", buf, sizeof buf);
        bk = bookAuto(b, name, buf[0] ? buf[0] : 'X', adv);
    } else {
        int r = atoi(buf) - 1;
        char *p = buf;
        while (isdigit((unsigned char)*p)) p++;
        int c = toupper((unsigned char)*p) - 'A';
        if (r < 0 || r >= b->rows || c < 0 || c >= b->cols) { printf("Invalid seat.\n"); return; }
        if (b->seat[r][c]) { printf("Seat already booked.\n"); return; }
        bk = addBooking(b, name, r, c, adv);
    }
    if (bk) { printf("\nBooking confirmed!\n"); printTicket(bk); }
}

static void bookGroupMenu(void) {
    Bus *b = getBus(readInt("Bus ID: "));
    if (!b) { printf("Invalid bus.\n"); return; }

    char name[NAME_LEN];
    readLine("Group / lead name: ", name, sizeof name);
    if (name[0] == '\0') { printf("Name required.\n"); return; }
    int n = readInt("Group size: ");
    int adv = readInt("Days in advance (0 if today): ");
    if (n < 1) { printf("Invalid group size.\n"); return; }
    if (adv < 0) adv = 0;

    if (freeSeats(b) < n) {
        enqueueWait(b, name, n, adv);
        printf("Not enough seats. Group added to waitlist (position %d).\n", b->waiting);
        return;
    }
    printf("\nBooking group of %d:\n", n);
    allocateGroup(b, name, n, adv);
}

static void cancelMenu(void) {
    Bus *b = getBus(readInt("Bus ID: "));
    if (!b) { printf("Invalid bus.\n"); return; }
    int id = readInt("Booking (ticket) ID to cancel: ");

    Booking *prev = NULL, *cur = b->bookings;
    while (cur && cur->id != id) { prev = cur; cur = cur->next; }
    if (!cur) { printf("Booking not found on this bus.\n"); return; }

    float refund = cur->price * REFUND_RATE;
    b->seat[cur->row][cur->col] = 0;
    b->booked--;
    b->revenue -= refund;
    if (prev) prev->next = cur->next; else b->bookings = cur->next;
    printf("Cancelled ticket #%d (%s, seat %d%c). Refund: Rs.%.2f\n", cur->id, cur->name,
           cur->row + 1, 'A' + cur->col, refund);
    free(cur);

    processWaitlist(b);
}

static void viewBookings(const Bus *b) {
    printf("\nBookings on Bus %d:\n", b->id);
    if (!b->bookings) { printf("  (none)\n"); return; }
    for (Booking *p = b->bookings; p; p = p->next) printTicket(p);
}

static void dailyReport(void) {
    float total = 0;
    int bookedAll = 0, seatsAll = 0;
    printf("\n================ DAILY REVENUE & OCCUPANCY REPORT ================\n");
    printf("%-4s %-24s %-8s %-10s %-10s %-6s\n", "Bus", "Route", "Booked", "Occupancy", "Revenue", "Wait");
    printf("------------------------------------------------------------------\n");
    for (int i = 0; i < busCount; i++) {
        Bus *b = &buses[i];
        char route[2 * ROUTE_LEN + 4];
        snprintf(route, sizeof route, "%s-%s", b->source, b->destination);
        printf("%-4d %-24.24s %2d/%-5d %6.1f%%    Rs.%-7.0f %-6d\n", b->id, route, b->booked,
               totalSeats(b), 100.0f * b->booked / totalSeats(b), b->revenue, b->waiting);
        total += b->revenue;
        bookedAll += b->booked;
        seatsAll += totalSeats(b);
    }
    printf("------------------------------------------------------------------\n");
    printf("TOTAL revenue: Rs.%.2f | Overall occupancy: %.1f%%\n", total,
           seatsAll ? 100.0f * bookedAll / seatsAll : 0.0f);
}

static void freeAll(void) {
    for (int i = 0; i < busCount; i++) {
        Booking *p = buses[i].bookings;
        while (p) { Booking *t = p; p = p->next; free(t); }
        WaitNode *w = buses[i].wlHead;
        while (w) { WaitNode *t = w; w = w->next; free(t); }
    }
}

int main(void) {
    initBus("Mathura", "Delhi",  "06:00", 8, 350);
    initBus("Mathura", "Agra",   "09:30", 6, 150);
    initBus("Delhi",   "Jaipur", "14:00", 5, 500);   

    int choice;
    do {
        printf("\n========== BUS SEAT RESERVATION SYSTEM ==========\n");
        printf(" 1. List all buses\n");
        printf(" 2. Search buses by route\n");
        printf(" 3. View seat map\n");
        printf(" 4. Book a seat (manual / auto)\n");
        printf(" 5. Book a group (contiguous seats)\n");
        printf(" 6. Cancel a booking\n");
        printf(" 7. View bookings of a bus\n");
        printf(" 8. View waitlist of a bus\n");
        printf(" 9. Daily revenue / occupancy report\n");
        printf(" 0. Exit\n");
        choice = readInt("Choice: ");

        switch (choice) {
            case 1: listBuses(); break;
            case 2: searchBuses(); break;
            case 3: { Bus *b = getBus(readInt("Bus ID: ")); b ? displaySeatMap(b) : (void)printf("Invalid bus.\n"); break; }
            case 4: bookSeatMenu(); break;
            case 5: bookGroupMenu(); break;
            case 6: cancelMenu(); break;
            case 7: { Bus *b = getBus(readInt("Bus ID: ")); b ? viewBookings(b) : (void)printf("Invalid bus.\n"); break; }
            case 8: { Bus *b = getBus(readInt("Bus ID: ")); b ? showWaitlist(b) : (void)printf("Invalid bus.\n"); break; }
            case 9: dailyReport(); break;
            case 0: printf("Goodbye!\n"); break;
            default: printf("Invalid choice.\n");
        }
    } while (choice != 0);

    freeAll();
    return 0;
}
