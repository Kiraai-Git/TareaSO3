#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

typedef enum {
    FIRST_FIT,
    BEST_FIT,
    WORST_FIT
} Policy;

typedef enum {
    BLOCK_FREE,
    BLOCK_OCCUPIED
} BlockType;

typedef struct Block {
    BlockType type;
    int pid; // -1 libre
    size_t size; // en bytes
    size_t address; 
    struct Block *prev;
    struct Block *next;
} Block;

typedef struct {
    Block *head;
    size_t total;
} MemList;

// Globales
static Policy policy;
static MemList mem;
static int alloc_count = 0;

static Block *block_new(BlockType type, int pid, size_t size, size_t address)
{
    Block *b = malloc(sizeof(Block));
    if (!b) {perror("malloc"); exit(EXIT_FAILURE);}
    b->type = type;
    b->pid = pid;
    b->size = size;
    b->address = address;
    b->prev = NULL;
    b->next = NULL;
    return b;
}

// Inicializar
static void mem_init(size_t total)
{
    mem.total = total;
    mem.head = block_new(BLOCK_FREE, -1, total, 0);
}

// Los 3 cases 
static Block *find_first_fit(size_t size)
{
    for (Block *b = mem.head; b; b = b->next)
        if (b->type == BLOCK_FREE && b->size >= size)
            return b;
    return NULL;
}

static Block *find_best_fit(size_t size)
{
    Block *best = NULL;
    size_t best_size = (size_t)-1;
    for (Block *b = mem.head; b; b = b->next) {
        if (b->type == BLOCK_FREE && b->size >= size && b->size < best_size) {
            best_size = b->size;
            best      = b;
        }
    }
    return best;
}

static Block *find_worst_fit(size_t size)
{
    Block *worst = NULL;
    size_t worst_size = 0;
    for (Block *b = mem.head; b; b = b->next) {
        if (b->type == BLOCK_FREE && b->size >= size && b->size > worst_size) {
            worst_size = b->size;
            worst      = b;
        }
    }
    return worst;
}

static Block *find_block(size_t size)
{
    switch (policy) {
        case FIRST_FIT: return find_first_fit(size);
        case BEST_FIT:  return find_best_fit(size);
        case WORST_FIT: return find_worst_fit(size);
    }
    return NULL;
}

// Malloc
static int mem_alloc(int pid, size_t size)
{
    Block *b = find_block(size);
    if (!b) return 0;

    if (b->size == size) {
        b->type = BLOCK_OCCUPIED;
        b->pid  = pid;
    } else {
        Block *remainder = block_new(BLOCK_FREE, -1, b->size - size, b->address + size);
        remainder->prev = b;
        remainder->next = b->next;
        if (b->next) b->next->prev = remainder;
        b->next = remainder;

        b->type = BLOCK_OCCUPIED;
        b->pid  = pid;
        b->size = size;
    }
    alloc_count++;
    return 1;
}


// Fusionar vacios contiguos
static Block *coalesce(Block *b)
{
    if (b->next && b->next->type == BLOCK_FREE) {
        Block *nxt = b->next;
        b->size   += nxt->size;
        b->next    = nxt->next;
        if (nxt->next) nxt->next->prev = b;
        free(nxt);
    }

    if (b->prev && b->prev->type == BLOCK_FREE) {
        Block *prv = b->prev;
        prv->size += b->size;
        prv->next  = b->next;
        if (b->next) b->next->prev = prv;
        free(b);
        return prv;
    }
    return b;
}


// Liberar
static void mem_free(int pid)
{
    Block *b = mem.head;
    while (b) {
        Block *next = b->next;
        if (b->type == BLOCK_OCCUPIED && b->pid == pid) {
            b->type = BLOCK_FREE;
            b->pid  = -1;
            b = coalesce(b);
        }
        b = b->next;
        (void)next;
    }
    b = mem.head;
    while (b) {
        if (b->type == BLOCK_FREE && b->next && b->next->type == BLOCK_FREE) {
            b = coalesce(b);
        } else {
            b = b->next;
        }
    }
}

// Compactar
static void mem_compact(void)
{
    Block *new_head = NULL;
    Block *new_tail = NULL;
    size_t free_total = 0;
    size_t addr = 0;

    Block *cur = mem.head;
    while (cur) {
        Block *next = cur->next;
        if (cur->type == BLOCK_OCCUPIED) {
            cur->address = addr;
            addr        += cur->size;
            cur->prev    = new_tail;
            cur->next    = NULL;
            if (new_tail) new_tail->next = cur;
            else          new_head = cur;
            new_tail = cur;
        } else {
            free_total += cur->size;
            free(cur);
        }
        cur = next;
    }

    if (free_total > 0) {
        Block *free_blk = block_new(BLOCK_FREE, -1, free_total, addr);
        free_blk->prev = new_tail;
        if (new_tail) new_tail->next = free_blk;
        else          new_head = free_blk;
        new_tail = free_blk;
        (void)new_tail;
    }

    mem.head = new_head;
}

// Info en consola
static void print_report(void)
{
    size_t used       = 0;
    size_t free_total = 0;
    size_t max_free   = 0;

    for (Block *b = mem.head; b; b = b->next) {
        if (b->type == BLOCK_OCCUPIED) {
            used += b->size;
        } else {
            free_total += b->size;
            if (b->size > max_free) max_free = b->size;
        }
    }

    double used_pct = (mem.total > 0)
                    ? (double)used / (double)mem.total * 100.0
                    : 0.0;

    double fext = 0.0;
    if (free_total > 0)
        fext = 1.0 - (double)max_free / (double)free_total;

    printf("Procesos asignados: %d\n", alloc_count);
    printf("Memoria utilizada: %.2f%%\n", used_pct);
    printf("Indice de Fragmentacion Externa: %.4f\n", fext);
    printf("Estado final de la memoria:\n");

    int first = 1;
    for (Block *b = mem.head; b; b = b->next) {
        if (!first) printf(" -> ");
        if (b->type == BLOCK_FREE)
            printf("[Libre %zu]", b->size);
        else
            printf("[Ocupado P%d %zu]", b->pid, b->size);
        first = 0;
    }
    printf("\n");
}

// Destruir
static void mem_destroy(void)
{
    Block *b = mem.head;
    while (b) {
        Block *next = b->next;
        free(b);
        b = next;
    }
    mem.head = NULL;
}

// Verificar el fit
static Policy parse_policy(const char *s)
{
    if (strcmp(s, "FIRST_FIT") == 0) return FIRST_FIT;
    if (strcmp(s, "BEST_FIT")  == 0) return BEST_FIT;
    if (strcmp(s, "WORST_FIT") == 0) return WORST_FIT;
    fprintf(stderr, "Error: politica desconocida '%s'\n", s);
    fprintf(stderr, "Opciones validas: FIRST_FIT, BEST_FIT, WORST_FIT\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <memoria_bytes> <POLITICA> <archivo_traza>\n", argv[0]);
        fprintf(stderr, "  Politicas: FIRST_FIT, BEST_FIT, WORST_FIT\n");
        return EXIT_FAILURE;
    }

    long mem_arg = atol(argv[1]);
    if (mem_arg <= 0) {
        fprintf(stderr, "Error: el tamanio de memoria debe ser positivo.\n");
        return EXIT_FAILURE;
    }
    size_t mem_size = (size_t)mem_arg;

    policy = parse_policy(argv[2]);
    mem_init(mem_size);

    FILE *fp = fopen(argv[3], "r");
    if (!fp) {
        perror(argv[3]);
        mem_destroy();
        return EXIT_FAILURE;
    }

    char line[256];
    int premature_exit = 0;

    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;

        char op[16] = {0};
        sscanf(line, "%15s", op);

        if (strcmp(op, "ALLOC") == 0) {
            int    pid  = 0;
            size_t size = 0;
            if (sscanf(line, "ALLOC %d %zu", &pid, &size) != 2) {
                fprintf(stderr, "Advertencia: linea malformada '%s'\n", line);
                continue;
            }
            if (!mem_alloc(pid, size)) {
                fprintf(stderr,
                    "ALLOC fallido para P%d (%zu bytes): "
                    "no hay bloque contiguo suficiente (fragmentacion externa).\n",
                    pid, size);
                premature_exit = 1;
                break;
            }

        } else if (strcmp(op, "FREE") == 0) {
            int pid = 0;
            if (sscanf(line, "FREE %d", &pid) != 1) {
                fprintf(stderr, "Advertencia: linea malformada '%s'\n", line);
                continue;
            }
            mem_free(pid);

        } else if (strcmp(op, "COMPACT") == 0) {
            mem_compact();

        } else {
            fprintf(stderr, "Operación desconocida '%s'\n", op);
        }
    }

    fclose(fp);

    if (premature_exit)
        fprintf(stderr, "Simulacion terminada antes de tiempo.\n");

    print_report();
    mem_destroy();
    return EXIT_SUCCESS;
}
