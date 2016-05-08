#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <curl/curl.h>

/* We'll need a mutex at some point to avoid race conditions */
std::mutex mtx;

/* Diry hack to avoid cURL polluting STDOUT */
size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp) {
    return size * nmemb;
}

/* Abstract type encoding a request result */
typedef struct {
    std::chrono::duration<double, std::milli> t; /* Time to process the request */
    long return_code; /* Return code of the request */
} Result;

void make_request(std::string url, int max_tries, std::vector<Result> *shared_results) {
    CURL *curl;
    CURLcode res;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

        for (int i = 0; i < max_tries; i++) {
            /* Start timer */
            auto t_start = std::chrono::high_resolution_clock::now();

            /* Make request */
            res = curl_easy_perform(curl);

            /* Stop timer */
            auto t_end = std::chrono::high_resolution_clock::now();

            /* Register result */
            Result result;
            result.t = std::chrono::duration<double, std::milli>(t_end - t_start);
            curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &result.return_code);

            mtx.lock();
            shared_results->push_back(result);
            mtx.unlock();
        }
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
}

void lt(std::string url, int n_threads, int n_requests) {
    std::vector<Result> results;
    std::vector<std::thread> threads; /* Handmade threadpool */

    for (int i = 0; i < n_threads; i++)
        threads.push_back(std::thread(make_request, url, n_requests, &results));

    std::cout << "[LT] Launched " << n_threads
              << " threads (" << n_requests
              << " requests each)." << std::endl;

    for (auto& th : threads)
        th.join();

    std::cout << "[LT] Finished." << std::endl;

    /* Average response time */
    double avg = 0.f;
    for (auto& res : results) {
        avg += res.t.count();
    }
    avg /= results.size();
    std::cout << "[LT] Average response time: " << avg << " ms" << std::endl;

    /* Number of successful calls */
    int success = 0;
    for (auto& res : results)
        if (res.return_code == 200)
            success++;
    std::cout << "[LT] " << success << " OK responses ("
              << (double)success / (double)results.size() * 100.f
              << "%)" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 4)
        std::cout << "Usage: " << argv[0] << " URL N_THREADS N_REQUESTS" << std::endl;
    else {
        std::string url(argv[1]);
        int n_threads = std::atoi(argv[2]);
        int n_requests = std::atoi(argv[3]);
        lt(url, n_threads, n_requests);
    }

    return EXIT_SUCCESS;
}
