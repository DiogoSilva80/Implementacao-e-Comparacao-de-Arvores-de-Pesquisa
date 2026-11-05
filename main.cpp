/**
 * AED - Projeto 2: Implementação e Comparação de Árvores de Pesquisa
 * ------------------------------------------------------------------
 * Este trabalho implementa quatro tipos de árvores (BST, AVL, RB e Splay)
 * para a gestão de transações financeiras.
 * Inclui otimizações de metadados para pesquisas por intervalo e
 * gestão de memória via pool estático para maior eficiência.
 */

#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <random>
#include <stack>
#include <string>
#include <memory>
#include <stdexcept>
#include <cassert>

using namespace std;

namespace SistemaFinanceiro {

    // Estrutura para armazenar os dados de cada transação
    struct Transaction {
        long id;
        int risk;
        Transaction(long _id, int _risk) : id(_id), risk(_risk) {}
    };

    enum class Color { RED, BLACK };

    // Nó universal para as diferentes estruturas de árvores
    struct Node {
        double value;                       // Valor monetário (chave)
        std::vector<Transaction> storage;    // Lista de transações com o mesmo valor
        int max_risk;                       // Risco máximo na subárvore (otimização)
        int node_max_risk;                  // Risco máximo local neste nó
        int height;                         // Campo para árvore AVL
        Color color;                        // Campo para árvore Red-Black
        Node *left, *right, *parent;        // Ponteiros de navegação

        Node() : value(0), max_risk(-1), node_max_risk(-1), height(0),
                 color(Color::BLACK), left(nullptr), right(nullptr), parent(nullptr) {}

        // Limpa e inicializa o nó para reutilização
        void reset(double v, long id, int r, Node* nil) {
            value = v;
            storage.clear();
            storage.emplace_back(id, r);
            max_risk = node_max_risk = r;
            height = 1;
            color = Color::RED;
            left = right = nil;
            parent = nil;
        }

        // Adiciona transação ao nó. Retorna true se o risco máximo local aumentar.
        bool add(long id, int r) {
            storage.emplace_back(id, r);
            if (r > node_max_risk) {
                node_max_risk = r;
                return true;
            }
            return false;
        }
    };

    // Gestor de memória para pré-alocação de nós
    class MemoryPool {
    private:
        vector<Node*> data;
        size_t allocation_ptr;
        size_t max_capacity;
    public:
        MemoryPool(size_t capacity) : allocation_ptr(0), max_capacity(capacity) {
            data.resize(capacity);
            for(size_t i = 0; i < capacity; ++i) {
                data[i] = new Node();
            }
        }

        // Obtém um nó disponível
        Node* acquire(double v, long id, int r, Node* nil) {
            if(allocation_ptr >= max_capacity) {
                Node* n = new Node();
                n->reset(v, id, r, nil);
                return n;
            }
            Node* n = data[allocation_ptr++];
            n->reset(v, id, r, nil);
            return n;
        }

        void recycle() { allocation_ptr = 0; }

        ~MemoryPool() {
            for(auto n : data) delete n;
        }
    };

    // Estrutura para armazenar métricas de execução
    struct Metrics {
        long long total_rotations = 0;
        double insertion_time = 0;
        double query_time = 0;
        double removal_time = 0;
        long found_count = 0;
        int final_depth = 0;
    };
}

using namespace SistemaFinanceiro;

// Classe base para as árvores
class ArvoreBase {
protected:
    Node* root;
    Node* NIL;
    Metrics stats;

    // Inicialização do nó sentinela para evitar ponteiros nulos
    void initSentinel() {
        NIL = static_cast<Node*>(::operator new(sizeof(Node)));
        NIL->value = -1.0;
        NIL->max_risk = NIL->node_max_risk = -1;
        NIL->height = 0;
        NIL->color = Color::BLACK;
        NIL->left = NIL->right = NIL->parent = NIL;
    }

    // Sincroniza metadados do nó com os filhos
    inline void sync(Node* n) {
        if (n == NIL || n == nullptr) return;

        int hL = n->left->height;
        int hR = n->right->height;
        n->height = 1 + (hL > hR ? hL : hR);

        int m = n->node_max_risk;
        if (n->left->max_risk > m) m = n->left->max_risk;
        if (n->right->max_risk > m) m = n->right->max_risk;
        n->max_risk = m;
    }

    // Propaga alterações de metadados para cima na árvore
    void propagateUp(Node* n, bool doHeight) {
        while (n != nullptr && n != NIL) {
            int old_r = n->max_risk;
            int old_h = n->height;
            sync(n);
            if (n->max_risk == old_r && (!doHeight || n->height == old_h)) {
                if (n->parent != NIL && n->parent->max_risk >= n->max_risk) break;
            }
            n = n->parent;
        }
    }

    // Rotação para a esquerda
    void rotateLeft(Node* x) {
        Node* y = x->right;
        x->right = y->left;
        if (y->left != NIL) y->left->parent = x;
        y->parent = x->parent;
        if (x->parent == NIL) root = y;
        else if (x == x->parent->left) x->parent->left = y;
        else x->parent->right = y;
        y->left = x;
        x->parent = y;
        sync(x);
        sync(y);
        stats.total_rotations++;
    }

    // Rotação para a direita
    void rotateRight(Node* y) {
        Node* x = y->left;
        y->left = x->right;
        if (x->right != NIL) x->right->parent = y;
        x->parent = y->parent;
        if (y->parent == NIL) root = x;
        else if (y == y->parent->right) y->parent->right = x;
        else y->parent->left = x;
        x->right = y;
        y->parent = x;
        sync(y);
        sync(x);
        stats.total_rotations++;
    }

    void transplant(Node* u, Node* v) {
        if (u->parent == NIL) root = v;
        else if (u == u->parent->left) u->parent->left = v;
        else u->parent->right = v;
        if (v != NIL) v->parent = u->parent;
    }

    Node* treeMinimum(Node* n) {
        while (n->left != NIL) n = n->left;
        return n;
    }

public:
    ArvoreBase() {
        initSentinel();
        root = NIL;
    }

    virtual ~ArvoreBase() {
        ::operator delete(NIL);
    }

    Metrics& getStats() { return stats; }
    void resetMetrics() { stats.total_rotations = 0; }
    long long getRotations() const { return stats.total_rotations; }

    // Pesquisa por intervalo com otimização de risco
    virtual vector<long> auditRange(double v1, double v2, int minR) {
        vector<long> results;
        if (root == NIL) return results;

        static vector<Node*> searchStack;
        searchStack.clear();
        searchStack.push_back(root);

        while (!searchStack.empty()) {
            Node* n = searchStack.back();
            searchStack.pop_back();

            // Poda da subárvore se o risco for insuficiente
            if (n->max_risk < minR) continue;

            if (n->value >= v1 && n->value <= v2) {
                if (n->node_max_risk >= minR) {
                    for (const auto& tx : n->storage)
                        if (tx.risk >= minR) results.push_back(tx.id);
                }
            }

            if (n->value < v2 && n->right != NIL) searchStack.push_back(n->right);
            if (n->value > v1 && n->left != NIL) searchStack.push_back(n->left);
        }
        return results;
    }

    virtual void insert(double v, long id, int r, MemoryPool& pool) = 0;
    virtual void remove(double v) = 0;

    // Garante que os metadados estão corretos de forma iterativa
    void forceIntegrityAudit() {
        if (root == NIL) return;
        vector<Node*> postOrder;
        vector<Node*> stack;
        stack.push_back(root);
        while(!stack.empty()){
            Node* curr = stack.back(); stack.pop_back();
            postOrder.push_back(curr);
            if(curr->left != NIL) stack.push_back(curr->left);
            if(curr->right != NIL) stack.push_back(curr->right);
        }
        for(auto it = postOrder.rbegin(); it != postOrder.rend(); ++it) sync(*it);
    }

    // Cálculo da altura máxima da árvore
    int calculateDepth() {
        if (root == NIL) return 0;
        int max_d = 0;
        stack<pair<Node*, int>> s;
        s.push({root, 1});
        while(!s.empty()){
            auto [curr, depth] = s.top(); s.pop();
            if (depth > max_d) max_d = depth;
            if (curr->left != NIL) s.push({curr->left, depth + 1});
            if (curr->right != NIL) s.push({curr->right, depth + 1});
        }
        return max_d;
    }
};

// Árvore Binária de Pesquisa com ponteiro de cauda
class EliteBST : public ArvoreBase {
    Node* tail;
public:
    EliteBST() : ArvoreBase(), tail(nullptr) {}

    void insert(double v, long id, int r, MemoryPool& pool) override {
        if (root == NIL) {
            root = tail = pool.acquire(v, id, r, NIL);
            return;
        }

        // Otimização para inserção ordenada
        if (v > tail->value) {
            Node* z = pool.acquire(v, id, r, NIL);
            tail->right = z;
            z->parent = tail;
            tail = z;
            propagateUp(z, false);
            return;
        }

        Node *y = NIL, *x = root;
        while (x != NIL) {
            y = x;
            if (v < x->value) x = x->left;
            else if (v > x->value) x = x->right;
            else {
                if (x->add(id, r)) propagateUp(x, false);
                return;
            }
        }
        Node* z = pool.acquire(v, id, r, NIL);
        z->parent = y;
        if (v < y->value) y->left = z;
        else {
            y->right = z;
            if (z->value > tail->value) tail = z;
        }
        propagateUp(z, false);
    }

    void remove(double v) override {
        Node* z = root;
        while (z != NIL && z->value != v) z = (v < z->value) ? z->left : z->right;
        if (z == NIL) return;

        Node* up;
        if (z->left == NIL) { up = z->parent; transplant(z, z->right); }
        else if (z->right == NIL) { up = z->parent; transplant(z, z->left); }
        else {
            Node* y = treeMinimum(z->right);
            up = (y->parent == z) ? y : y->parent;
            if (y->parent != z) { transplant(y, y->right); y->right = z->right; y->right->parent = y; }
            transplant(z, y);
            y->left = z->left; y->left->parent = y;
        }
        if (tail == z) tail = nullptr;
        propagateUp(up, false);
    }
};

// Árvore AVL
class EliteAVL : public ArvoreBase {
private:
    Node* balanceNode(Node* n) {
        sync(n);
        int factor = n->left->height - n->right->height;

        if (factor > 1) {
            if (n->left->right->height > n->left->left->height) {
                rotateLeft(n->left);
            }
            rotateRight(n);
            return n->parent;
        }

        if (factor < -1) {
            if (n->right->left->height > n->right->right->height) {
                rotateRight(n->right);
            }
            rotateLeft(n);
            return n->parent;
        }
        return n;
    }

    Node* insertRec(Node* n, double v, long id, int r, MemoryPool& pool) {
        if (n == NIL) return pool.acquire(v, id, r, NIL);

        if (v < n->value) {
            n->left = insertRec(n->left, v, id, r, pool);
            n->left->parent = n;
        } else if (v > n->value) {
            n->right = insertRec(n->right, v, id, r, pool);
            n->right->parent = n;
        } else {
            n->add(id, r);
            sync(n);
            return n;
        }
        return balanceNode(n);
    }

    Node* removeRec(Node* n, double v) {
        if (n == NIL) return NIL;

        if (v < n->value) n->left = removeRec(n->left, v);
        else if (v > n->value) n->right = removeRec(n->right, v);
        else {
            if (n->left == NIL || n->right == NIL) {
                Node* t = (n->left != NIL) ? n->left : n->right;
                if (t != NIL) t->parent = n->parent;
                return t;
            }
            Node* y = treeMinimum(n->right);
            n->value = y->value;
            n->storage = y->storage;
            n->node_max_risk = y->node_max_risk;
            n->right = removeRec(n->right, y->value);
        }
        return balanceNode(n);
    }

public:
    void insert(double v, long id, int r, MemoryPool& pool) override {
        root = insertRec(root, v, id, r, pool);
        root->parent = NIL;
    }
    void remove(double v) override {
        root = removeRec(root, v);
        if (root != NIL) root->parent = NIL;
    }
};

// Árvore Vermelha e Preta
class EliteRB : public ArvoreBase {
private:
    void fixInsert(Node* k) {
        while (k->parent->color == Color::RED) {
            if (k->parent == k->parent->parent->left) {
                Node* uncle = k->parent->parent->right;
                if (uncle->color == Color::RED) {
                    uncle->color = Color::BLACK;
                    k->parent->color = Color::BLACK;
                    k->parent->parent->color = Color::RED;
                    k = k->parent->parent;
                } else {
                    if (k == k->parent->right) {
                        k = k->parent;
                        rotateLeft(k);
                    }
                    k->parent->color = Color::BLACK;
                    k->parent->parent->color = Color::RED;
                    rotateRight(k->parent->parent);
                }
            } else {
                Node* uncle = k->parent->parent->left;
                if (uncle->color == Color::RED) {
                    uncle->color = Color::BLACK;
                    k->parent->color = Color::BLACK;
                    k->parent->parent->color = Color::RED;
                    k = k->parent->parent;
                } else {
                    if (k == k->parent->left) {
                        k = k->parent;
                        rotateRight(k);
                    }
                    k->parent->color = Color::BLACK;
                    k->parent->parent->color = Color::RED;
                    rotateLeft(k->parent->parent);
                }
            }
            if (k == root) break;
        }
        root->color = Color::BLACK;
    }

    void fixDelete(Node* x, Node* xp) {
        while (x != root && (x == NIL || x->color == Color::BLACK)) {
            if (x == xp->left) {
                Node* w = xp->right;
                if (w->color == Color::RED) {
                    w->color = Color::BLACK; xp->color = Color::RED;
                    rotateLeft(xp); w = xp->right;
                }
                if ((w->left == NIL || w->left->color == Color::BLACK) &&
                    (w->right == NIL || w->right->color == Color::BLACK)) {
                    w->color = Color::RED; x = xp; xp = x->parent;
                } else {
                    if (w->right == NIL || w->right->color == Color::BLACK) {
                        if (w->left != NIL) w->left->color = Color::BLACK;
                        w->color = Color::RED; rotateRight(w); w = xp->right;
                    }
                    w->color = xp->color; xp->color = Color::BLACK;
                    if (w->right != NIL) w->right->color = Color::BLACK;
                    rotateLeft(xp); x = root;
                }
            } else {
                Node* w = xp->left;
                if (w->color == Color::RED) {
                    w->color = Color::BLACK; xp->color = Color::RED;
                    rotateRight(xp); w = xp->left;
                }
                if ((w->right == NIL || w->right->color == Color::BLACK) &&
                    (w->left == NIL || w->left->color == Color::BLACK)) {
                    w->color = Color::RED; x = xp; xp = x->parent;
                } else {
                    if (w->left == NIL || w->left->color == Color::BLACK) {
                        if (w->right != NIL) w->right->color = Color::BLACK;
                        w->color = Color::RED; rotateLeft(w); w = xp->left;
                    }
                    w->color = xp->color; xp->color = Color::BLACK;
                    if (w->left != NIL) w->left->color = Color::BLACK;
                    rotateRight(xp); x = root;
                }
            }
        }
        if (x != NIL) x->color = Color::BLACK;
    }

public:
    void insert(double v, long id, int r, MemoryPool& pool) override {
        Node *y = NIL, *x = root;
        while (x != NIL) {
            y = x;
            if (v < x->value) x = x->left; else if (v > x->value) x = x->right;
            else { if (x->add(id, r)) propagateUp(x, false); return; }
        }
        Node* z = pool.acquire(v, id, r, NIL);
        z->parent = y;
        if (y == NIL) root = z; else if (v < y->value) y->left = z; else y->right = z;
        propagateUp(z, false);
        fixInsert(z);
        propagateUp(z, false);
    }

    void remove(double v) override {
        Node* z = root;
        while (z != NIL && z->value != v) z = (v < z->value) ? z->left : z->right;
        if (z == NIL) return;

        Node *y = z, *x, *xp; Color original_color = y->color;
        if (z->left == NIL) {
            x = z->right; xp = z->parent; transplant(z, z->right);
        } else if (z->right == NIL) {
            x = z->left; xp = z->parent; transplant(z, z->left);
        } else {
            y = treeMinimum(z->right); original_color = y->color; x = y->right;
            if (y->parent == z) { if (x != NIL) x->parent = y; xp = y; }
            else { xp = y->parent; transplant(y, y->right); y->right = z->right; y->right->parent = y; }
            transplant(z, y); y->left = z->left; y->left->parent = y; y->color = z->color;
        }
        propagateUp(xp, false);
        if (original_color == Color::BLACK) fixDelete(x, xp);
    }
};

// Árvore Splay
class EliteSplay : public ArvoreBase {
private:
    void splay(Node* x) {
        if (!x || x == NIL) return;
        while (x->parent != NIL) {
            Node *p = x->parent, *g = p->parent;
            if (g == NIL) {
                if (x == p->left) rotateRight(p); else rotateLeft(p);
            } else if (x == p->left && p == g->left) {
                rotateRight(g); rotateRight(p);
            } else if (x == p->right && p == g->right) {
                rotateLeft(g); rotateLeft(p);
            } else if (x == p->right && p == g->left) {
                rotateLeft(p); rotateRight(g);
            } else {
                rotateRight(p); rotateLeft(g);
            }
        }
        sync(root);
    }
public:
    vector<long> auditRange(double v1, double v2, int minR) override {
        if (root == NIL) return {};
        Node *curr = root, *last = NIL;
        while (curr != NIL) {
            last = curr;
            if (v1 < curr->value) curr = curr->left;
            else if (v1 > curr->value) curr = curr->right;
            else break;
        }
        splay(last);
        return ArvoreBase::auditRange(v1, v2, minR);
    }

    void insert(double v, long id, int r, MemoryPool& pool) override {
        Node *y = NIL, *x = root;
        while (x != NIL) {
            y = x;
            if (v < x->value) x = x->left; else if (v > x->value) x = x->right;
            else { if (x->add(id, r)) propagateUp(x, false); splay(x); return; }
        }
        Node* z = pool.acquire(v, id, r, NIL);
        z->parent = y;
        if (y == NIL) root = z; else if (v < y->value) y->left = z; else y->right = z;
        propagateUp(z, false);
        splay(z);
    }

    void remove(double v) override {
        Node* z = root;
        while (z != NIL && z->value != v) z = (v < z->value) ? z->left : z->right;
        if (z == NIL) return;
        splay(z);
        Node *L = z->left, *R = z->right;
        if (L != NIL) L->parent = NIL; if (R != NIL) R->parent = NIL;
        if (L == NIL) root = R;
        else {
            Node* m = L; while (m->right != NIL) m = m->right;
            root = L; splay(m);
            root->right = R; if (R != NIL) R->parent = root;
            sync(root);
        }
    }
};

// Funções de teste e análise
struct InputSet { double val; int risk; };

void runAnalysis(ArvoreBase* tree, string name, const vector<InputSet>& ins, const vector<pair<double, double>>& qrs, MemoryPool& pool) {
    cout << "\n>>> ANALISANDO ESTRUTURA: " << left << setw(12) << name << " <<<" << endl;
    pool.recycle(); tree->resetMetrics();

    // Inserção
    auto start = chrono::high_resolution_clock::now();
    for (size_t i = 0; i < ins.size(); i++) {
        tree->insert(ins[i].val, (long)i, ins[i].risk, pool);
    }
    auto end = chrono::high_resolution_clock::now();
    tree->getStats().insertion_time = chrono::duration<double>(end - start).count();
    long long insRot = tree->getRotations();

    tree->forceIntegrityAudit();
    tree->getStats().final_depth = tree->calculateDepth();

    // Consulta
    start = chrono::high_resolution_clock::now();
    for (const auto& q : qrs) {
        tree->getStats().found_count += tree->auditRange(q.first, q.second, 4).size();
    }
    end = chrono::high_resolution_clock::now();
    tree->getStats().query_time = chrono::duration<double>(end - start).count();

    // Remoção
    tree->resetMetrics();
    start = chrono::high_resolution_clock::now();
    for (size_t i = 0; i < ins.size() / 5; i++) {
        tree->remove(ins[i].val);
    }
    end = chrono::high_resolution_clock::now();
    tree->getStats().removal_time = chrono::duration<double>(end - start).count();
    long long remRot = tree->getRotations();

    cout << "  [F1] Insercao: " << fixed << setprecision(4) << tree->getStats().insertion_time << "s | Rotacoes: " << insRot << endl;
    cout << "  [F2] Consulta: " << tree->getStats().query_time << "s | Achados: " << tree->getStats().found_count << endl;
    cout << "  [F3] Remocao : " << tree->getStats().removal_time << "s | Rotacoes: " << remRot << " | Altura: " << tree->getStats().final_depth << endl;
}

int main() {
    ios_base::sync_with_stdio(false); cin.tie(NULL);

    const int N = 100000;
    MemoryPool pool(N + 40000);
    mt19937 rng(42);

    // Geração de inputs
    vector<InputSet> randomData, sortedData, duplicateData;
    for(int i=0; i<N; i++) {
        randomData.push_back({(double)(rng() % 1000000) / 10.0, (int)(rng() % 5 + 1)});
        sortedData.push_back({(double)i, (int)(rng() % 5 + 1)});
        duplicateData.push_back({(double)(rng() % 500), (int)(rng() % 10 + 1)});
    }

    // Geração de consultas
    vector<pair<double, double>> queries;
    for(int i=0; i<10000; i++) {
        double v1 = (rng() % 500000) / 10.0;
        queries.push_back({v1, v1 + 1000.0});
    }

    cout << "==================================================================" << endl;
    cout << "           AED PROJETO 2: ANALISE DE ARVORES DE PESQUISA          " << endl;
    cout << "==================================================================" << endl;

    cout << "\n--- TESTE 1: DADOS ALEATORIOS ---" << endl;
    { EliteBST b; runAnalysis(&b, "BST", randomData, queries, pool); }
    { EliteAVL a; runAnalysis(&a, "AVL", randomData, queries, pool); }
    { EliteRB r; runAnalysis(&r, "RED-BLACK", randomData, queries, pool); }
    { EliteSplay s; runAnalysis(&s, "SPLAY", randomData, queries, pool); }

    cout << "\n--- TESTE 2: DADOS ORDENADOS ---" << endl;
    { EliteBST b; runAnalysis(&b, "BST", sortedData, queries, pool); }
    { EliteAVL a; runAnalysis(&a, "AVL", sortedData, queries, pool); }
    { EliteRB r; runAnalysis(&r, "RED-BLACK", sortedData, queries, pool); }
    { EliteSplay s; runAnalysis(&s, "SPLAY", sortedData, queries, pool); }

    cout << "\n--- TESTE 3: ALTA DUPLICIDADE ---" << endl;
    { EliteBST b; runAnalysis(&b, "BST", duplicateData, queries, pool); }
    { EliteAVL a; runAnalysis(&a, "AVL", duplicateData, queries, pool); }
    { EliteRB r; runAnalysis(&r, "RED-BLACK", duplicateData, queries, pool); }
    { EliteSplay s; runAnalysis(&s, "SPLAY", duplicateData, queries, pool); }

    cout << "\n==================================================================" << endl;
    cout << "             TESTES FINALIZADOS COM SUCESSO                       " << endl;
    cout << "==================================================================" << endl;

    return 0;
}