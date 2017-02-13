#include <iostream>
#include <set>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <stack>
#include <mutex>
#include <future>
#include <unordered_map>

typedef std::set<int> MyClassSet;
typedef std::vector<int> MyClassSVector;
typedef std::unordered_map<int, MyClassSVector> MyClassVectorMap;
typedef std::unordered_map<int,int> MyClassIntMap;
typedef std::unordered_map<int,double> MyClassDoubleMap;

typedef std::map<int,double> MyClassDoubleMapToResults;

/*
 * Trzyma wszystkie wierzchołki jakie zostały wczytane
 * */
MyClassSet vertices;

/*
 * Dla każdego wierzchołka trzyma liste jego krawędzi
 * */
MyClassVectorMap edges;
/*
 * Nieposortowane wartości 'betweenness centrality'
 * */
MyClassDoubleMap BC;

/*
 * Zmienna do uporzadkowania wartosci rosnaco
 * */
MyClassDoubleMapToResults BC_order;

std::mutex mut1;
unsigned int threads_number;

/*
 * wrzytywanie danych z wejscia
 * */
void read_data(MyClassSet &vertices,MyClassVectorMap &edges, const std::string &file){

    std::ifstream infile(file);
    std::string line;

    while (std::getline(infile,line)){
        std::istringstream iss (line);
        int a;
        int b;
        iss >> a;
        iss >> b;

        vertices.insert(a);
        vertices.insert(b);

        if(edges.find(a) == edges.end())
            edges.insert(std::pair<int, std::vector<int>>(a,{}));
        edges.at(a).push_back(b);
    }
}

/*
 * Funkcja przeprowadzajaca obliczenia dla danego wierzcholka 's'
 * Tresc taka jak w pseudokodzie algorytmu
 * */
void single_calculate(int s,MyClassDoubleMap &BC){

    std::stack<int> S;
    MyClassVectorMap P;
    MyClassIntMap sigma;
    MyClassIntMap d;
    MyClassDoubleMap delta;

    for(auto it : vertices){
        sigma.insert(std::pair<int,int>(it,0));
        d.insert(std::pair<int,int>(it,-1));
        delta.insert(std::pair<int,double>(it,0.0));
    }

    if(sigma.find(s) == sigma.end())
        sigma.insert(std::pair<int,int>(s,1));
    else
        sigma.at(s) = 1;

    if(d.find(s) == d.end())
        d.insert(std::pair<int,int>(s,0));
    else
        d.at(s) = 0;

    std::deque<int> Q;
    Q.push_back(s);

    int v;
    while (!Q.empty()){
        v = Q.front();
        Q.pop_front();
        S.push(v);
        if(edges.find(v) != edges.end()) {
            for (auto w : edges.at(v)) {
                if (d.at(w) < 0) {
                    Q.push_back(w);
                    d.at(w) = d.at(v) + 1;
                }

                if (d.at(w) == d.at(v) + 1) {
                    sigma.at(w) += sigma.at(v);

                    if (P.find(w) == P.end())
                        P.insert(std::pair<int, std::vector<int>>(w, {}));

                    P.at(w).push_back(v);
                }
            }
        }
    }

    int w1;
    while (!S.empty()){
        w1 = S.top();
        S.pop();

        if(P.find(w1) != P.end()) {
            for (auto v1 : P.at(w1))
                delta.at(v1) += ((double)sigma.at(v1) / sigma.at(w1)) * (1 + delta.at(w1));
        }

        if(w1 != s){
            if(BC.find(w1) == BC.end())
                BC.insert(std::pair<int,double>(w1,0.0));
            BC.at(w1) += delta.at(w1);
        }
    }
}

/*
 * Funkcja wykonujaca obliczenia dla jednego watku
 * dla wielu watkow funkcje te dzialaja dopuki istnieja wezly dla ktorych
 * nie przeprowadzono obliczen
 * */
void thread_function(std::deque<int> &vertices_to_use, std::promise<MyClassDoubleMap> &promise) {

    MyClassDoubleMap BC_tmp;

    while(true) {

        int v;

        {
            std::lock_guard<std::mutex> lock(mut1);
            if (vertices_to_use.empty())
                break;
            else {
                v = vertices_to_use.front();
                vertices_to_use.pop_front();
            }
        }
        single_calculate(v,BC_tmp);
    }
    promise.set_value(BC_tmp);
}

/*
 * Funkcja czekajaca na zakonczenie wszystkich watkow
 * */
void wait_for_threads(std::vector<std::thread> &threads, std::vector<std::promise<MyClassDoubleMap>> &promis_map) {

    for (unsigned int i = 0; i < threads.size(); ++i) {
        std::future<MyClassDoubleMap> future = promis_map[i].get_future();
        MyClassDoubleMap result = future.get();

        for (auto it : result) {
            BC[it.first] += it.second;
        }

        threads[i].join();
    }
}

/*
 * Funkcja glowna gdy liczymy tylko jednym watkiem
 * */
void one_thread() {

    for (auto v : vertices) {
        single_calculate(v,BC);
    }
}

/*
 * Funkcja glowna gdy liczymy wieloma watkami
 * */
void many_threads() {

    std::deque<int> vertices_to_use(vertices.begin(), vertices.end());
    std::vector<std::promise<MyClassDoubleMap>> promis_map(threads_number);
    std::vector<std::thread> threads;

    for (int i = 0; i < threads_number; ++i) {
        threads.push_back(std::thread([&vertices_to_use, &promis_map, i] {
            thread_function(vertices_to_use, promis_map[i]);
        }));
    }
    wait_for_threads(threads, promis_map);
}

/*
 * Funkcja porzadkujaca wyniki
 * */
void make_order(){
    for(auto it : BC)
        BC_order.insert(it);
}

int main(int argc, char *argv[]) {

    threads_number = (unsigned int) std::atoi(argv[1]) - 1;

    std::string in(argv[2]);
    std::string out(argv[3]);

    read_data(vertices,edges,in);

    for(auto it : vertices)
        BC.insert(std::pair<int,double>(it,0.0));

    if (threads_number == 0)
        one_thread();
    else
        many_threads();

    make_order();

    std::ofstream output_file(out);
    for (auto it : BC_order) {
        if (edges.find(it.first) != edges.end()) {
            output_file << it.first << " " << it.second << std::endl;
        }
    }

    return 0;
}