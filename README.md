# Projeto #2: Implementação e Comparação de Árvores de Pesquisa - Motor Financeiro Vanguarda



**Unidade Curricular:** Algoritmos e Estruturas de Dados (AED)

**Autor:** Diogo Silva



## Descrição do Projeto



Este projeto foca-se no desenvolvimento e análise de um motor de gestão de transações financeiras, onde cada registo é definido pelo triplo (ID, Valor, Risco). O sistema opera em três fases distintas para avaliar a performance das estruturas de dados num ambiente de stress: inserção massiva, consulta por intervalo monetário (com filtro de risco) e remoção por valor.



O objetivo central foi avaliar o impacto do balanceamento e das otimizações na performance de quatro estruturas de dados baseadas em Árvores de Pesquisa Binária (BST).



## Estruturas de Dados e Decisões Específicas



Para lidar com valores não únicos, implementou-se o *Node Grouping*, onde cada nó armazena uma lista de transações (`std::vector<Transaction> storage`) com o mesmo valor monetário, permitindo que a remoção seja atómica para todos os registos do mesmo valor.



As quatro árvores implementadas foram:



* **Elite BST (Binary Search Tree):** Implementada com *Tail-Pointer Optimization* (ponteiro para o nó de maior valor), mitigando a degeneração para $O(N^2)$ e garantindo inserções sequenciais no pior caso (Cenário Ordenado) em $O(1)$ amortizado.



* **Elite AVL:** Garante um rebalanceamento estrito através do campo `height`, assegurando que a altura nunca exceda $1.44 \log N$. As rotações duplas são contabilizadas como duas rotações simples.



* **Elite Red-Black:** Segue a implementação clássica (CLRS), utilizando o campo `color` e um nó sentinela (`NIL`). Implementa os 8 casos de reestruturação na remoção, garantindo uma altura máxima de $2 \log(N+1)$.



* **Elite Splay:** Estrutura auto-ajustável (*Bottom-Up*). Adotou-se a política *Splay-on-Search* na Fase 2 para otimizar a localidade de acesso, contabilizando *Zig* como 1 rotação e *Zig-Zig*/*Zig-Zag* como 2.



## Otimizações Adicionais



* **Poda (Pruning) por Risco Máximo:** A pesquisa por intervalo foi otimizada com a aumentação de metadados. Cada nó guarda o risco máximo (`max_risk`) da sua subárvore. Durante as consultas da Fase 2, se o `max_risk` de um ramo for inferior ao risco mínimo exigido, todo o ramo é descartado. Isto transforma uma pesquisa potencialmente $O(N)$ num limite de $O(\log N + K)$, garantindo os mesmos resultados em todas as estruturas.



* **Gestão de Memória (Memory Pool):** Utilizou-se pré-alocação estática (pool) para mitigar o custo computacional de invocações sequenciais na criação de novos nós durante a inserção, promovendo a reutilização eficiente da memória e estabilizando os testes de desempenho.



## Geração de Dados e Cenários de Teste



O projeto submete as árvores a dois cenários cruciais (Fase 1) gerados estocasticamente:



1. **Cenário Aleatório (Caso Médio):** 100.000 transações com valores distribuídos uniformemente.



2. **Cenário Ordenado (Caso Crítico):** 100.000 transações inseridas sequencialmente (0, 1, 2...). Representa o pior caso para árvores não balanceadas e testa a eficiência do sistema de rotações (AVL e Red-Black).



## Resultados Empíricos e Conclusões



Os testes demonstraram a superioridade das estruturas balanceadas em sistemas de larga escala:



* **Impacto da Poda no Pior Caso (Fase 2):** No cenário ordenado (altura 100.000), a árvore Red-Black efetuou 10.000 consultas em 0.26s face a 4.48s da BST. A poda na BST foi ineficaz na listagem linear, enquanto na Red-Black (balanceamento logarítmico) conseguiu evitar a análise de milhões de transações irrelevantes.



* **Equilíbrio Velocidade vs Balanceamento:** Na remoção (Fase 3), a AVL apresentou uma altura final inferior à Red-Black (17 vs 31), sinalizando um rebalanceamento estrito mais agressivo. No entanto, a Red-Black cimentou-se como a estrutura mais rápida em operações de escrita (remoção em 0.0011s contra 0.0028s da AVL), revelando-se a estrutura ideal para este caso de uso.



## Como Compilar e Executar



1. Certifica-te de que tens um compilador de C++ compatível com a norma C++11 (ex: `g++`).



2. Navega até ao diretório do código no teu terminal de sistema.



3. Compila e otimiza o código com o seguinte comando:

   ```bash

   g++ -O3 -std=c++11 main_2.cpp -o avaliacao_arvores

   ```



4. Executa o binário resultante:

   ```bash

   ./avaliacao_arvores

