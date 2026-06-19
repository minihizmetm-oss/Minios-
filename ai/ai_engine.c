#include "../include/types.h"

extern void *kmalloc(size_t s);

typedef struct {
    float weights[8];
    float bias;
    float output;
} perceptron_t;

typedef struct {
    float alpha;
    float value;
    int count;
} ewma_t;

static perceptron_t cpu_pred;
static ewma_t load_avg;
static float prediction_confidence = 0.5f;

float sigmoid(float x) {
    if(x > 10) return 1;
    if(x < -10) return 0;
    return x / (1 + (x < 0 ? -x : x));
}

void ai_engine_init(void) {
    for(int i=0; i<8; i++) cpu_pred.weights[i] = 0.1f;
    cpu_pred.bias = 0;
    load_avg.alpha = 0.3f;
    load_avg.value = 0;
    load_avg.count = 0;
}

float ai_predict_cpu_load(float *features) {
    float sum = cpu_pred.bias;
    for(int i=0; i<8; i++) sum += features[i] * cpu_pred.weights[i];
    cpu_pred.output = sigmoid(sum);
    
    if(load_avg.count == 0) load_avg.value = cpu_pred.output;
    else load_avg.value = load_avg.alpha * cpu_pred.output + (1-load_avg.alpha)*load_avg.value;
    load_avg.count++;
    
    prediction_confidence = 0.5f + (load_avg.count > 100 ? 0.3f : load_avg.count*0.003f);
    return cpu_pred.output;
}

void ai_update_model(float *features, float actual) {
    float error = actual - cpu_pred.output;
    for(int i=0; i<8; i++) cpu_pred.weights[i] += 0.01f * error * features[i];
    cpu_pred.bias += 0.01f * error;
}

float ai_get_confidence(void) { return prediction_confidence; }

/* ============================================================
 * GELİŞMİŞ AI - Anomali Tespiti + EWMA + LSTM
 * ============================================================ */

/* Anomali tespit sistemi */
typedef struct {
    float mean;
    float variance;
    float threshold;
    int count;
    float last_zscore;
    int alert_level;
    int anomaly_count;
} anomaly_t;

static anomaly_t anomaly_det;

void anomaly_init(float threshold) {
    anomaly_det.mean = 0;
    anomaly_det.variance = 0;
    anomaly_det.threshold = threshold;
    anomaly_det.count = 0;
    anomaly_det.last_zscore = 0;
    anomaly_det.alert_level = 0;
    anomaly_det.anomaly_count = 0;
}

int anomaly_detect(float value) {
    anomaly_det.count++;
    float delta = value - anomaly_det.mean;
    anomaly_det.mean += delta / anomaly_det.count;
    float delta2 = value - anomaly_det.mean;
    anomaly_det.variance += delta * delta2;
    
    if(anomaly_det.count < 10) return 0;
    
    float stddev = anomaly_det.variance / anomaly_det.count;
    if(stddev < 0.001f) stddev = 0.001f;
    
    /* sqrt yaklaşık */
    float x = stddev;
    float sqrt_stddev = x;
    for(int i=0; i<5; i++) sqrt_stddev = (sqrt_stddev + x/sqrt_stddev) / 2.0f;
    
    float zscore = (value - anomaly_det.mean) / sqrt_stddev;
    if(zscore < 0) zscore = -zscore;
    
    anomaly_det.last_zscore = zscore;
    
    if(zscore > anomaly_det.threshold * 2.0f) {
        anomaly_det.alert_level = 2;
        anomaly_det.anomaly_count++;
        return 2;
    } else if(zscore > anomaly_det.threshold) {
        anomaly_det.alert_level = 1;
        anomaly_det.anomaly_count++;
        return 1;
    }
    
    anomaly_det.alert_level = 0;
    return 0;
}

/* LSTM hücresi - basitleştirilmiş */
typedef struct {
    float *hidden_state;
    float *cell_state;
    int hidden_size;
    float forget_bias;
    float input_bias;
    float output_bias;
    float cell_bias;
} lstm_simple_t;

lstm_simple_t *lstm_create(int hidden_size) {
    lstm_simple_t *lstm = kmalloc(sizeof(lstm_simple_t));
    if(!lstm) return NULL;
    
    lstm->hidden_size = hidden_size;
    lstm->hidden_state = kmalloc(hidden_size * sizeof(float));
    lstm->cell_state = kmalloc(hidden_size * sizeof(float));
    
    for(int i=0; i<hidden_size; i++) {
        lstm->hidden_state[i] = 0;
        lstm->cell_state[i] = 0;
    }
    
    lstm->forget_bias = 1.0f;
    lstm->input_bias = 0;
    lstm->output_bias = 0;
    lstm->cell_bias = 0;
    
    return lstm;
}

void lstm_step(lstm_simple_t *lstm, float input) {
    for(int i=0; i<lstm->hidden_size; i++) {
        /* Forget gate */
        float f = sigmoid(lstm->forget_bias + input * 0.1f);
        /* Input gate */
        float in = sigmoid(lstm->input_bias + input * 0.1f);
        /* Output gate */
        float o = sigmoid(lstm->output_bias + input * 0.1f);
        /* Cell candidate */
        float c_cand = sigmoid(lstm->cell_bias + input * 0.1f) * 2 - 1;
        
        lstm->cell_state[i] = f * lstm->cell_state[i] + in * c_cand;
        lstm->hidden_state[i] = o * (lstm->cell_state[i] > 0 ? lstm->cell_state[i] : 0);
    }
}

/* AI istatistikleri */
void ai_print_stats(void) {
    volatile char *serial = (volatile char *)0x3F8;
    char *hdr = "\n=== AI ENGINE STATS ===\n";
    while(*hdr) *serial = *hdr++;
    
    char *p1 = "Confidence: ";
    while(*p1) *serial = *p1++;
    int conf = (int)(prediction_confidence * 100);
    *serial = '0' + conf/10;
    *serial = '0' + conf%10;
    *serial = '%'; *serial = '\n';
    
    char *p2 = "Anomalies: ";
    while(*p2) *serial = *p2++;
    *serial = '0' + anomaly_det.anomaly_count;
    *serial = '\n';
    
    char *p3 = "EWMA: ";
    while(*p3) *serial = *p3++;
    int ewma = (int)(load_avg.value * 100);
    *serial = '0' + ewma/10;
    *serial = '0' + ewma%10;
    *serial = '%'; *serial = '\n';
}

/* ============================================================
 * REINFORCEMENT LEARNING (Q-Learning)
 * ============================================================ */

#define Q_STATES 16
#define Q_ACTIONS 4

typedef struct {
    float q_table[Q_STATES][Q_ACTIONS];
    float lr;
    float discount;
    float epsilon;
    int episodes;
    float total_reward;
} qlearning_t;

static qlearning_t *ql = NULL;

void ql_init(float lr, float discount, float epsilon) {
    ql = kmalloc(sizeof(qlearning_t));
    if(!ql) return;
    
    for(int s=0; s<Q_STATES; s++)
        for(int a=0; a<Q_ACTIONS; a++)
            ql->q_table[s][a] = 0;
    
    ql->lr = lr;
    ql->discount = discount;
    ql->epsilon = epsilon;
    ql->episodes = 0;
    ql->total_reward = 0;
}

int ql_select_action(int state) {
    if(!ql) return 0;
    
    /* Epsilon-greedy */
    if((state % 100) / 100.0f < ql->epsilon) {
        return state % Q_ACTIONS;
    }
    
    int best = 0;
    float best_val = ql->q_table[state][0];
    for(int a=1; a<Q_ACTIONS; a++) {
        if(ql->q_table[state][a] > best_val) {
            best_val = ql->q_table[state][a];
            best = a;
        }
    }
    return best;
}

void ql_update(int state, int action, float reward, int next_state) {
    if(!ql) return;
    
    float max_next = ql->q_table[next_state][0];
    for(int a=1; a<Q_ACTIONS; a++)
        if(ql->q_table[next_state][a] > max_next)
            max_next = ql->q_table[next_state][a];
    
    ql->q_table[state][action] += ql->lr * (reward + ql->discount * max_next - ql->q_table[state][action]);
    ql->total_reward += reward;
    ql->episodes++;
}

/* ============================================================
 * K-MEANS KÜMELEME
 * ============================================================ */

#define KM_MAX_K 8
#define KM_MAX_POINTS 64

typedef struct {
    float centroids[KM_MAX_K][8];
    int assignments[KM_MAX_POINTS];
    float points[KM_MAX_POINTS][8];
    int k;
    int dim;
    int num_points;
} kmeans_t;

static kmeans_t *km = NULL;

void km_init(int k, int dim) {
    km = kmalloc(sizeof(kmeans_t));
    if(!km) return;
    
    km->k = k;
    km->dim = dim;
    km->num_points = 0;
    
    for(int c=0; c<k; c++)
        for(int d=0; d<dim; d++)
            km->centroids[c][d] = (float)(c*7 + d*13) / 100.0f;
}

void km_add_point(float *features) {
    if(!km || km->num_points >= KM_MAX_POINTS) return;
    for(int d=0; d<km->dim; d++)
        km->points[km->num_points][d] = features[d];
    km->num_points++;
}

void km_cluster(void) {
    if(!km || km->num_points == 0) return;
    
    for(int iter=0; iter<50; iter++) {
        /* Assign */
        for(int p=0; p<km->num_points; p++) {
            float min_dist = 1e10f;
            int best = 0;
            for(int c=0; c<km->k; c++) {
                float dist = 0;
                for(int d=0; d<km->dim; d++) {
                    float diff = km->points[p][d] - km->centroids[c][d];
                    dist += diff * diff;
                }
                if(dist < min_dist) { min_dist = dist; best = c; }
            }
            km->assignments[p] = best;
        }
        
        /* Update centroids */
        for(int c=0; c<km->k; c++) {
            float sum[8] = {0};
            int count = 0;
            for(int p=0; p<km->num_points; p++) {
                if(km->assignments[p] == c) {
                    for(int d=0; d<km->dim; d++) sum[d] += km->points[p][d];
                    count++;
                }
            }
            if(count > 0)
                for(int d=0; d<km->dim; d++)
                    km->centroids[c][d] = sum[d] / count;
        }
    }
}

int km_predict(float *features) {
    if(!km) return 0;
    float min_dist = 1e10f;
    int best = 0;
    for(int c=0; c<km->k; c++) {
        float dist = 0;
        for(int d=0; d<km->dim; d++) {
            float diff = features[d] - km->centroids[c][d];
            dist += diff * diff;
        }
        if(dist < min_dist) { min_dist = dist; best = c; }
    }
    return best;
}

/* ============================================================
 * GENETİK ALGORİTMA
 * ============================================================ */

#define GA_POP 32
#define GA_GENES 16

typedef struct {
    float genes[GA_GENES];
    float fitness;
} individual_t;

typedef struct {
    individual_t pop[GA_POP];
    int gene_count;
    float mutation_rate;
    int generation;
} genetic_t;

static genetic_t *ga = NULL;

void ga_init(int gene_count, float mutation_rate) {
    ga = kmalloc(sizeof(genetic_t));
    if(!ga) return;
    
    ga->gene_count = gene_count;
    ga->mutation_rate = mutation_rate;
    ga->generation = 0;
    
    for(int i=0; i<GA_POP; i++) {
        for(int g=0; g<gene_count; g++)
            ga->pop[i].genes[g] = (float)(i*7+g*13) / 1000.0f - 0.5f;
        ga->pop[i].fitness = 0;
    }
}

void ga_evaluate(float (*fitness_func)(float*)) {
    if(!ga) return;
    for(int i=0; i<GA_POP; i++)
        ga->pop[i].fitness = fitness_func(ga->pop[i].genes);
}

void ga_evolve(void) {
    if(!ga) return;
    
    /* Sort by fitness (bubble) */
    for(int i=0; i<GA_POP-1; i++)
        for(int j=0; j<GA_POP-i-1; j++)
            if(ga->pop[j].fitness < ga->pop[j+1].fitness) {
                individual_t tmp = ga->pop[j];
                ga->pop[j] = ga->pop[j+1];
                ga->pop[j+1] = tmp;
            }
    
    /* Crossover top 50% */
    for(int i=GA_POP/2; i<GA_POP; i+=2) {
        int p1 = (i * 7) % (GA_POP/2);
        int p2 = (i * 13) % (GA_POP/2);
        int cross = i % ga->gene_count;
        
        for(int g=0; g<ga->gene_count; g++) {
            if(g < cross) ga->pop[i].genes[g] = ga->pop[p1].genes[g];
            else ga->pop[i].genes[g] = ga->pop[p2].genes[g];
            
            /* Mutation */
            if((g*17+i*23) % 100 < (int)(ga->mutation_rate*100))
                ga->pop[i].genes[g] += (float)(i*g) / 10000.0f - 0.05f;
        }
    }
    
    ga->generation++;
}

float *ga_get_best(void) {
    if(!ga) return NULL;
    return ga->pop[0].genes;
}

/* ============================================================
 * BAYES SINIFLANDIRICI
 * ============================================================ */

typedef struct {
    float prior;
    float mean;
    float variance;
} bayes_class_t;

static bayes_class_t bayes[2];  /* 2 sınıf: normal/anormal */

void bayes_init(void) {
    bayes[0].prior = 0.5f; bayes[0].mean = 0; bayes[0].variance = 1;
    bayes[1].prior = 0.5f; bayes[1].mean = 0; bayes[1].variance = 1;
}

void bayes_train(int class, float value) {
    if(class < 0 || class > 1) return;
    bayes[class].mean = (bayes[class].mean * 0.9f) + (value * 0.1f);
    float diff = value - bayes[class].mean;
    bayes[class].variance = (bayes[class].variance * 0.9f) + (diff * diff * 0.1f);
}

int bayes_predict(float value) {
    float p0 = bayes[0].prior;
    float p1 = bayes[1].prior;
    
    /* Gaussian PDF yaklaşık */
    float d0 = (value - bayes[0].mean);
    d0 = (d0 * d0) / (2 * bayes[0].variance + 0.01f);
    p0 *= 1.0f / (1.0f + d0);
    
    float d1 = (value - bayes[1].mean);
    d1 = (d1 * d1) / (2 * bayes[1].variance + 0.01f);
    p1 *= 1.0f / (1.0f + d1);
    
    return p1 > p0 ? 1 : 0;
}

/* ============================================================
 * LİNEER REGRESYON
 * ============================================================ */

typedef struct {
    float slope;
    float intercept;
    float r_squared;
    int count;
    float sum_x, sum_y, sum_xy, sum_x2, sum_y2;
} linear_regression_t;

static linear_regression_t linreg;

void linreg_init(void) {
    linreg.slope = 0;
    linreg.intercept = 0;
    linreg.r_squared = 0;
    linreg.count = 0;
    linreg.sum_x = linreg.sum_y = linreg.sum_xy = linreg.sum_x2 = linreg.sum_y2 = 0;
}

void linreg_add_point(float x, float y) {
    linreg.count++;
    linreg.sum_x += x;
    linreg.sum_y += y;
    linreg.sum_xy += x * y;
    linreg.sum_x2 += x * x;
    linreg.sum_y2 += y * y;
    
    if(linreg.count >= 2) {
        float n = linreg.count;
        float denom = n * linreg.sum_x2 - linreg.sum_x * linreg.sum_x;
        if(denom != 0) {
            linreg.slope = (n * linreg.sum_xy - linreg.sum_x * linreg.sum_y) / denom;
            linreg.intercept = (linreg.sum_y - linreg.slope * linreg.sum_x) / n;
            
            /* R-squared */
            float ss_res = 0, ss_tot = 0;
            float mean_y = linreg.sum_y / n;
            ss_res = linreg.sum_y2 - linreg.intercept * linreg.sum_y - linreg.slope * linreg.sum_xy;
            ss_tot = linreg.sum_y2 - linreg.sum_y * linreg.sum_y / n;
            linreg.r_squared = 1 - ss_res / (ss_tot + 0.001f);
        }
    }
}

float linreg_predict(float x) {
    return linreg.slope * x + linreg.intercept;
}

/* ============================================================
 * ZAMAN SERİSİ TAHMİNİ (ARIMA benzeri)
 * ============================================================ */

#define TS_MAX_HISTORY 256

typedef struct {
    float history[TS_MAX_HISTORY];
    int history_idx;
    int history_count;
    float trend;
    float seasonal[4];
    float last_prediction;
    float error_accum;
} time_series_t;

static time_series_t ts;

void ts_init(void) {
    for(int i=0; i<TS_MAX_HISTORY; i++) ts.history[i] = 0;
    ts.history_idx = 0;
    ts.history_count = 0;
    ts.trend = 0;
    for(int i=0; i<4; i++) ts.seasonal[i] = 0;
    ts.last_prediction = 0;
    ts.error_accum = 0;
}

void ts_add_value(float value) {
    ts.history[ts.history_idx] = value;
    ts.history_idx = (ts.history_idx + 1) % TS_MAX_HISTORY;
    if(ts.history_count < TS_MAX_HISTORY) ts.history_count++;
    
    /* Trend hesapla (basit moving average) */
    if(ts.history_count >= 10) {
        float recent = 0, older = 0;
        for(int i=0; i<5; i++) {
            int idx1 = (ts.history_idx - 1 - i + TS_MAX_HISTORY) % TS_MAX_HISTORY;
            int idx2 = (ts.history_idx - 6 - i + TS_MAX_HISTORY) % TS_MAX_HISTORY;
            recent += ts.history[idx1];
            older += ts.history[idx2];
        }
        ts.trend = (recent - older) / 5.0f;
    }
}

float ts_predict(int steps_ahead) {
    if(ts.history_count < 2) return 0;
    
    /* Simple exponential smoothing with trend */
    int last_idx = (ts.history_idx - 1 + TS_MAX_HISTORY) % TS_MAX_HISTORY;
    float last_value = ts.history[last_idx];
    float prediction = last_value + ts.trend * steps_ahead;
    
    /* Error correction */
    if(ts.last_prediction != 0) {
        float error = last_value - ts.last_prediction;
        ts.error_accum = ts.error_accum * 0.9f + error * 0.1f;
        prediction += ts.error_accum;
    }
    
    ts.last_prediction = prediction;
    return prediction;
}

/* ============================================================
 * ÇOK DEĞİŞKENLİ REGRESYON
 * ============================================================ */

#define MV_MAX_FEATURES 8

typedef struct {
    float coefficients[MV_MAX_FEATURES + 1]; /* +1 for intercept */
    int num_features;
    int trained;
} multi_regression_t;

static multi_regression_t mreg;

void mreg_init(int num_features) {
    if(num_features > MV_MAX_FEATURES) num_features = MV_MAX_FEATURES;
    mreg.num_features = num_features;
    mreg.trained = 0;
    for(int i=0; i<=num_features; i++) mreg.coefficients[i] = 0;
    mreg.coefficients[0] = 0.5f; /* Intercept */
}

void mreg_train(float *features, float target) {
    float prediction = mreg.coefficients[0];
    for(int i=0; i<mreg.num_features; i++)
        prediction += mreg.coefficients[i+1] * features[i];
    
    float error = target - prediction;
    float lr = 0.01f;
    
    mreg.coefficients[0] += lr * error;
    for(int i=0; i<mreg.num_features; i++)
        mreg.coefficients[i+1] += lr * error * features[i];
    
    mreg.trained = 1;
}

float mreg_predict(float *features) {
    float result = mreg.coefficients[0];
    for(int i=0; i<mreg.num_features; i++)
        result += mreg.coefficients[i+1] * features[i];
    return result;
}

/* ============================================================
 * KARAR AĞACI (ID3 benzeri)
 * ============================================================ */

#define DT_MAX_NODES 32

typedef struct dt_node {
    int feature;
    float threshold;
    float value;
    int is_leaf;
    int left_idx;
    int right_idx;
} dt_node_t;

static dt_node_t dt_nodes[DT_MAX_NODES];
static int dt_node_count = 0;

void dt_init(void) {
    dt_node_count = 0;
    for(int i=0; i<DT_MAX_NODES; i++) {
        dt_nodes[i].feature = -1;
        dt_nodes[i].threshold = 0;
        dt_nodes[i].value = 0;
        dt_nodes[i].is_leaf = 1;
        dt_nodes[i].left_idx = -1;
        dt_nodes[i].right_idx = -1;
    }
}

int dt_add_leaf(float value) {
    if(dt_node_count >= DT_MAX_NODES) return -1;
    int idx = dt_node_count++;
    dt_nodes[idx].is_leaf = 1;
    dt_nodes[idx].value = value;
    return idx;
}

int dt_add_split(int feature, float threshold, int left, int right) {
    if(dt_node_count >= DT_MAX_NODES) return -1;
    int idx = dt_node_count++;
    dt_nodes[idx].is_leaf = 0;
    dt_nodes[idx].feature = feature;
    dt_nodes[idx].threshold = threshold;
    dt_nodes[idx].left_idx = left;
    dt_nodes[idx].right_idx = right;
    return idx;
}

float dt_predict_tree(float *features) {
    int node = 0;
    while(!dt_nodes[node].is_leaf) {
        if(features[dt_nodes[node].feature] <= dt_nodes[node].threshold)
            node = dt_nodes[node].left_idx;
        else
            node = dt_nodes[node].right_idx;
        if(node < 0 || node >= dt_node_count) return 0;
    }
    return dt_nodes[node].value;
}

/* ============================================================
 * NORMALİZASYON VE ÖLÇEKLENDİRME
 * ============================================================ */

typedef struct {
    float min;
    float max;
    float range;
} minmax_scaler_t;

static minmax_scaler_t scalers[8];

void scaler_fit(int idx, float min, float max) {
    scalers[idx].min = min;
    scalers[idx].max = max;
    scalers[idx].range = max - min;
    if(scalers[idx].range < 0.001f) scalers[idx].range = 0.001f;
}

float scaler_transform(int idx, float value) {
    return (value - scalers[idx].min) / scalers[idx].range;
}

float scaler_inverse(int idx, float value) {
    return value * scalers[idx].range + scalers[idx].min;
}

/* ============================================================
 * PERFORMANS METRİKLERİ
 * ============================================================ */

typedef struct {
    float mae;   /* Mean Absolute Error */
    float mse;   /* Mean Squared Error */
    float rmse;  /* Root Mean Squared Error */
    float mape;  /* Mean Absolute Percentage Error */
    int count;
    float sum_abs_error;
    float sum_sq_error;
    float sum_abs_pct;
} metrics_t;

static metrics_t ai_metrics;

void metrics_init(void) {
    ai_metrics.mae = ai_metrics.mse = ai_metrics.rmse = ai_metrics.mape = 0;
    ai_metrics.count = ai_metrics.sum_abs_error = ai_metrics.sum_sq_error = ai_metrics.sum_abs_pct = 0;
}

void metrics_update(float actual, float predicted) {
    float error = actual - predicted;
    if(error < 0) error = -error;
    
    ai_metrics.count++;
    ai_metrics.sum_abs_error += error;
    ai_metrics.sum_sq_error += error * error;
    
    if(actual != 0) {
        ai_metrics.sum_abs_pct += error / (actual > 0 ? actual : -actual);
    }
    
    ai_metrics.mae = ai_metrics.sum_abs_error / ai_metrics.count;
    ai_metrics.mse = ai_metrics.sum_sq_error / ai_metrics.count;
    
    /* sqrt yaklaşık */
    float x = ai_metrics.mse;
    ai_metrics.rmse = x;
    for(int i=0; i<5; i++) ai_metrics.rmse = (ai_metrics.rmse + x/ai_metrics.rmse) / 2.0f;
    
    ai_metrics.mape = ai_metrics.sum_abs_pct / ai_metrics.count * 100;
}

float metrics_get_accuracy(void) {
    if(ai_metrics.rmse < 0.001f) return 100;
    return 100 - ai_metrics.mape;
}

/* ============================================================
 * RANDOM FOREST (Basitleştirilmiş)
 * ============================================================ */

#define RF_MAX_TREES 8
#define RF_MAX_FEATURES 4

typedef struct {
    float split_value;
    int feature;
    float prediction;
    int is_leaf;
} rf_tree_t;

typedef struct {
    rf_tree_t trees[RF_MAX_TREES];
    int num_trees;
    float feature_importances[RF_MAX_FEATURES];
} random_forest_t;

static random_forest_t rf;

void rf_init(int num_trees) {
    if(num_trees > RF_MAX_TREES) num_trees = RF_MAX_TREES;
    rf.num_trees = num_trees;
    for(int i=0; i<num_trees; i++) {
        rf.trees[i].is_leaf = 1;
        rf.trees[i].prediction = 0;
        rf.trees[i].feature = i % RF_MAX_FEATURES;
        rf.trees[i].split_value = 0.5f;
    }
    for(int i=0; i<RF_MAX_FEATURES; i++) rf.feature_importances[i] = 1.0f/RF_MAX_FEATURES;
}

void rf_train_tree(int tree_idx, float *features, float target) {
    if(tree_idx >= rf.num_trees) return;
    rf.trees[tree_idx].prediction = rf.trees[tree_idx].prediction * 0.9f + target * 0.1f;
    rf.trees[tree_idx].is_leaf = 0;
    
    /* Feature importance güncelle */
    int feat = tree_idx % RF_MAX_FEATURES;
    rf.feature_importances[feat] = rf.feature_importances[feat] * 0.95f + 0.05f;
}

float rf_predict(float *features) {
    float sum = 0;
    int count = 0;
    for(int i=0; i<rf.num_trees; i++) {
        if(!rf.trees[i].is_leaf) {
            sum += rf.trees[i].prediction;
            count++;
        }
    }
    return count > 0 ? sum / count : 0;
}

int rf_get_most_important_feature(void) {
    int best = 0;
    float best_val = rf.feature_importances[0];
    for(int i=1; i<RF_MAX_FEATURES; i++) {
        if(rf.feature_importances[i] > best_val) {
            best_val = rf.feature_importances[i];
            best = i;
        }
    }
    return best;
}

/* ============================================================
 * GRADIENT DESCENT OPTIMIZER
 * ============================================================ */

typedef struct {
    float *params;
    float *velocity;
    float *momentum;
    int num_params;
    float learning_rate;
    float momentum_rate;
    float decay;
    int iterations;
} gd_optimizer_t;

static gd_optimizer_t *gd = NULL;

gd_optimizer_t *gd_create(int num_params, float lr) {
    gd_optimizer_t *opt = kmalloc(sizeof(gd_optimizer_t));
    if(!opt) return NULL;
    
    opt->params = kmalloc(num_params * sizeof(float));
    opt->velocity = kmalloc(num_params * sizeof(float));
    opt->momentum = kmalloc(num_params * sizeof(float));
    opt->num_params = num_params;
    opt->learning_rate = lr;
    opt->momentum_rate = 0.9f;
    opt->decay = 0.0001f;
    opt->iterations = 0;
    
    for(int i=0; i<num_params; i++) {
        opt->params[i] = 0;
        opt->velocity[i] = 0;
        opt->momentum[i] = 0;
    }
    
    return opt;
}

void gd_step(gd_optimizer_t *opt, float *gradients) {
    if(!opt) return;
    
    float lr = opt->learning_rate / (1 + opt->decay * opt->iterations);
    opt->iterations++;
    
    for(int i=0; i<opt->num_params; i++) {
        /* Momentum */
        opt->momentum[i] = opt->momentum_rate * opt->momentum[i] - lr * gradients[i];
        /* Velocity (RMSprop benzeri) */
        opt->velocity[i] = opt->velocity[i] * 0.9f + gradients[i] * gradients[i] * 0.1f;
        /* Update */
        float adapt_lr = lr / (1.0f + opt->velocity[i] > 0 ? 1.0f + opt->velocity[i] : 1.0f);
        if(adapt_lr < 0.0001f) adapt_lr = 0.0001f;
        opt->params[i] += opt->momentum[i] * adapt_lr;
    }
}

/* ============================================================
 * CONFUSION MATRIX
 * ============================================================ */

typedef struct {
    int tp;  /* True Positive */
    int tn;  /* True Negative */
    int fp;  /* False Positive */
    int fn;  /* False Negative */
    int total;
} confusion_matrix_t;

static confusion_matrix_t cm;

void cm_init(void) {
    cm.tp = cm.tn = cm.fp = cm.fn = cm.total = 0;
}

void cm_add(int actual, int predicted) {
    cm.total++;
    if(actual == 1 && predicted == 1) cm.tp++;
    else if(actual == 0 && predicted == 0) cm.tn++;
    else if(actual == 0 && predicted == 1) cm.fp++;
    else if(actual == 1 && predicted == 0) cm.fn++;
}

float cm_accuracy(void) {
    if(cm.total == 0) return 0;
    return (float)(cm.tp + cm.tn) / cm.total;
}

float cm_precision(void) {
    if(cm.tp + cm.fp == 0) return 0;
    return (float)cm.tp / (cm.tp + cm.fp);
}

float cm_recall(void) {
    if(cm.tp + cm.fn == 0) return 0;
    return (float)cm.tp / (cm.tp + cm.fn);
}

float cm_f1_score(void) {
    float p = cm_precision();
    float r = cm_recall();
    if(p + r == 0) return 0;
    return 2 * p * r / (p + r);
}

/* ============================================================
 * ÖZELLİK MÜHENDİSLİĞİ (Feature Engineering)
 * ============================================================ */

#define FE_MAX_WINDOW 32

typedef struct {
    float window[FE_MAX_WINDOW];
    int window_idx;
    int window_size;
} feature_engine_t;

static feature_engine_t fe;

void fe_init(int window_size) {
    if(window_size > FE_MAX_WINDOW) window_size = FE_MAX_WINDOW;
    fe.window_size = window_size;
    fe.window_idx = 0;
    for(int i=0; i<FE_MAX_WINDOW; i++) fe.window[i] = 0;
}

void fe_add_value(float value) {
    fe.window[fe.window_idx] = value;
    fe.window_idx = (fe.window_idx + 1) % fe.window_size;
}

/* Moving Average */
float fe_moving_average(int period) {
    if(period > fe.window_size) period = fe.window_size;
    float sum = 0;
    int count = 0;
    for(int i=0; i<period; i++) {
        int idx = (fe.window_idx - 1 - i + fe.window_size) % fe.window_size;
        sum += fe.window[idx];
        count++;
    }
    return count > 0 ? sum / count : 0;
}

/* Rate of Change */
float fe_rate_of_change(void) {
    int last = (fe.window_idx - 1 + fe.window_size) % fe.window_size;
    int prev = (fe.window_idx - 2 + fe.window_size) % fe.window_size;
    if(fe.window[prev] == 0) return 0;
    return (fe.window[last] - fe.window[prev]) / fe.window[prev];
}

/* Momentum indicator */
float fe_momentum(int period) {
    float current = fe_moving_average(1);
    float past = fe_moving_average(period);
    return current - past;
}

/* Volatility (standard deviation) */
float fe_volatility(int period) {
    if(period > fe.window_size) period = fe.window_size;
    float mean = fe_moving_average(period);
    float sum_sq = 0;
    int count = 0;
    for(int i=0; i<period; i++) {
        int idx = (fe.window_idx - 1 - i + fe.window_size) % fe.window_size;
        float diff = fe.window[idx] - mean;
        sum_sq += diff * diff;
        count++;
    }
    if(count < 2) return 0;
    float var = sum_sq / (count - 1);
    /* sqrt */
    float result = var;
    for(int i=0; i<5; i++) result = (result + var/result) / 2.0f;
    return result;
}

/* ============================================================
 * VERİ ÖN İŞLEME
 * ============================================================ */

/* One-Hot Encoding (basit) */
void ai_one_hot_encode(int value, int max_categories, float *output) {
    for(int i=0; i<max_categories; i++) output[i] = 0;
    if(value >= 0 && value < max_categories) output[value] = 1.0f;
}

/* Normalize (Min-Max) */
void ai_normalize(float *data, int count, float *output) {
    float min = data[0], max = data[0];
    for(int i=1; i<count; i++) {
        if(data[i] < min) min = data[i];
        if(data[i] > max) max = data[i];
    }
    float range = max - min;
    if(range < 0.001f) range = 0.001f;
    for(int i=0; i<count; i++) output[i] = (data[i] - min) / range;
}

/* Standardize (Z-score) */
void ai_standardize(float *data, int count, float *output) {
    float sum = 0, sum_sq = 0;
    for(int i=0; i<count; i++) {
        sum += data[i];
        sum_sq += data[i] * data[i];
    }
    float mean = sum / count;
    float var = sum_sq/count - mean*mean;
    float stddev = var;
    for(int i=0; i<5; i++) stddev = (stddev + var/stddev) / 2.0f;
    if(stddev < 0.001f) stddev = 0.001f;
    for(int i=0; i<count; i++) output[i] = (data[i] - mean) / stddev;
}

/* ============================================================
 * DERİN SİNİR AĞI (DNN)
 * ============================================================ */

#define DNN_MAX_LAYERS 8
#define DNN_MAX_NEURONS 128

typedef struct {
    float *weights;
    float *bias;
    float *output;
    float *delta;
    int inputs;
    int neurons;
    int activation;
} dnn_layer_t;

typedef struct {
    dnn_layer_t layers[DNN_MAX_LAYERS];
    int num_layers;
    float learning_rate;
    float momentum;
    int trained;
} deep_network_t;

static deep_network_t *dnn = NULL;

void dnn_create(int *layer_sizes, int num_layers, float lr) {
    dnn = kmalloc(sizeof(deep_network_t));
    if(!dnn || num_layers > DNN_MAX_LAYERS) return;
    
    dnn->num_layers = num_layers;
    dnn->learning_rate = lr;
    dnn->momentum = 0.9f;
    dnn->trained = 0;
    
    for(int l=0; l<num_layers; l++) {
        dnn_layer_t *layer = &dnn->layers[l];
        layer->neurons = layer_sizes[l];
        layer->inputs = (l==0) ? layer_sizes[0] : layer_sizes[l-1];
        
        int w_size = layer->inputs * layer->neurons;
        layer->weights = kmalloc(w_size * sizeof(float));
        layer->bias = kmalloc(layer->neurons * sizeof(float));
        layer->output = kmalloc(layer->neurons * sizeof(float));
        layer->delta = kmalloc(layer->neurons * sizeof(float));
        layer->activation = (l == num_layers-1) ? 0 : 1; /* 0=sigmoid, 1=relu */
        
        for(int i=0; i<w_size; i++)
            layer->weights[i] = ((float)(i*7+l*13) / 10000.0f) - 0.5f;
        for(int i=0; i<layer->neurons; i++)
            layer->bias[i] = 0.1f;
    }
}

float *dnn_predict(float *input) {
    if(!dnn) return NULL;
    
    float *current = input;
    for(int l=0; l<dnn->num_layers; l++) {
        dnn_layer_t *layer = &dnn->layers[l];
        
        for(int n=0; n<layer->neurons; n++) {
            float sum = layer->bias[n];
            for(int i=0; i<layer->inputs; i++)
                sum += current[i] * layer->weights[i * layer->neurons + n];
            
            if(layer->activation == 0)
                layer->output[n] = 1.0f/(1.0f + (sum<-10?1:sum>10?1:sum/(1+(sum<0?-sum:sum))));
            else
                layer->output[n] = sum > 0 ? sum : 0;
        }
        current = layer->output;
    }
    return dnn->layers[dnn->num_layers-1].output;
}

/* ============================================================
 * XGBOOST BENZERİ (Gradient Boosting)
 * ============================================================ */

#define XGB_MAX_TREES 32
#define XGB_MAX_DEPTH 6

typedef struct {
    float split_value;
    int feature;
    float leaf_value;
    int is_leaf;
    int left;
    int right;
} xgb_tree_node_t;

typedef struct {
    xgb_tree_node_t nodes[64];
    int node_count;
    float learning_rate;
    float lambda; /* L2 regularization */
    int depth;
} xgb_tree_t;

typedef struct {
    xgb_tree_t trees[XGB_MAX_TREES];
    int num_trees;
    float base_prediction;
} xgboost_t;

static xgboost_t *xgb = NULL;

void xgb_init(float lr, float lambda) {
    xgb = kmalloc(sizeof(xgboost_t));
    if(!xgb) return;
    xgb->num_trees = 0;
    xgb->base_prediction = 0.5f;
}

void xgb_add_tree(void) {
    if(!xgb || xgb->num_trees >= XGB_MAX_TREES) return;
    xgb_tree_t *tree = &xgb->trees[xgb->num_trees];
    tree->node_count = 0;
    tree->learning_rate = 0.1f;
    tree->lambda = 1.0f;
    tree->depth = 0;
    
    /* Basit karar ağacı */
    tree->nodes[0].is_leaf = 1;
    tree->nodes[0].leaf_value = 0;
    tree->node_count = 1;
    
    xgb->num_trees++;
}

float xgb_predict(float *features) {
    if(!xgb) return xgb ? xgb->base_prediction : 0;
    float pred = xgb->base_prediction;
    for(int t=0; t<xgb->num_trees; t++) {
        /* Her ağacın tahminini ekle */
        xgb_tree_t *tree = &xgb->trees[t];
        int node = 0;
        while(!tree->nodes[node].is_leaf) {
            if(features[tree->nodes[node].feature] <= tree->nodes[node].split_value)
                node = tree->nodes[node].left;
            else
                node = tree->nodes[node].right;
        }
        pred += tree->learning_rate * tree->nodes[node].leaf_value;
    }
    return pred;
}

/* ============================================================
 * SVM (Support Vector Machine)
 * ============================================================ */

#define SVM_MAX_VECTORS 64
#define SVM_MAX_FEATURES 8

typedef struct {
    float support_vectors[SVM_MAX_VECTORS][SVM_MAX_FEATURES];
    float alphas[SVM_MAX_VECTORS];
    float bias;
    int num_vectors;
    int num_features;
    float C; /* Regularization */
    float gamma; /* RBF kernel gamma */
} svm_t;

static svm_t *svm = NULL;

void svm_init(int features, float C, float gamma) {
    svm = kmalloc(sizeof(svm_t));
    if(!svm) return;
    svm->num_features = features;
    svm->num_vectors = 0;
    svm->C = C;
    svm->gamma = gamma;
    svm->bias = 0;
}

void svm_add_vector(float *features, float alpha) {
    if(!svm || svm->num_vectors >= SVM_MAX_VECTORS) return;
    for(int i=0; i<svm->num_features; i++)
        svm->support_vectors[svm->num_vectors][i] = features[i];
    svm->alphas[svm->num_vectors] = alpha;
    svm->num_vectors++;
}

/* RBF Kernel */
static float svm_rbf(float *x1, float *x2) {
    float dist = 0;
    for(int i=0; i<svm->num_features; i++) {
        float diff = x1[i] - x2[i];
        dist += diff * diff;
    }
    /* exp(-gamma * dist) yaklaşık */
    float val = -svm->gamma * dist;
    return 1.0f / (1.0f + (val < 0 ? -val : val));
}

float svm_predict(float *features) {
    if(!svm || svm->num_vectors == 0) return 0;
    float sum = -svm->bias;
    for(int i=0; i<svm->num_vectors; i++)
        sum += svm->alphas[i] * svm_rbf(svm->support_vectors[i], features);
    return sum > 0 ? 1 : -1;
}

/* ============================================================
 * MODEL BİRLEŞTİRME (Stacking Ensemble)
 * ============================================================ */

typedef struct {
    float *weights;
    int num_models;
    float bias;
} stacking_t;

static stacking_t *stack = NULL;

void stack_init(int num_models) {
    stack = kmalloc(sizeof(stacking_t));
    if(!stack) return;
    stack->num_models = num_models;
    stack->weights = kmalloc(num_models * sizeof(float));
    stack->bias = 0;
    for(int i=0; i<num_models; i++)
        stack->weights[i] = 1.0f / num_models;
}

float stack_predict(float *predictions) {
    if(!stack) return 0;
    float sum = stack->bias;
    for(int i=0; i<stack->num_models; i++)
        sum += stack->weights[i] * predictions[i];
    return sum;
}

void stack_update(float *predictions, float target, float lr) {
    if(!stack) return;
    float pred = stack_predict(predictions);
    float error = target - pred;
    stack->bias += lr * error;
    for(int i=0; i<stack->num_models; i++)
        stack->weights[i] += lr * error * predictions[i];
}

/* ============================================================
 * DROPOUT REGULARIZATION
 * ============================================================ */

typedef struct {
    uint8_t *mask;
    int size;
    float rate;
} dropout_t;

dropout_t *dropout_create(int size, float rate) {
    dropout_t *dp = kmalloc(sizeof(dropout_t));
    if(!dp) return NULL;
    dp->mask = kmalloc(size * sizeof(uint8_t));
    dp->size = size;
    dp->rate = rate;
    for(int i=0; i<size; i++) dp->mask[i] = 1;
    return dp;
}

void dropout_apply(dropout_t *dp, float *data) {
    if(!dp) return;
    for(int i=0; i<dp->size; i++) {
        /* Rastgele dropout */
        if((i*17 + 31) % 100 < (int)(dp->rate*100))
            dp->mask[i] = 0;
        else
            dp->mask[i] = 1;
        data[i] *= dp->mask[i];
    }
}

/* ============================================================
 * ERKEN DURDURMA (Early Stopping)
 * ============================================================ */

typedef struct {
    float best_loss;
    int patience;
    int wait;
    int stop;
    float *best_weights;
    int num_weights;
} early_stopping_t;

early_stopping_t *early_stop_create(int num_weights, int patience) {
    early_stopping_t *es = kmalloc(sizeof(early_stopping_t));
    if(!es) return NULL;
    es->best_loss = 999999.0f;
    es->patience = patience;
    es->wait = 0;
    es->stop = 0;
    es->num_weights = num_weights;
    es->best_weights = kmalloc(num_weights * sizeof(float));
    return es;
}

int early_stop_check(early_stopping_t *es, float current_loss, float *weights) {
    if(!es) return 0;
    if(current_loss < es->best_loss) {
        es->best_loss = current_loss;
        es->wait = 0;
        for(int i=0; i<es->num_weights; i++)
            es->best_weights[i] = weights[i];
    } else {
        es->wait++;
        if(es->wait >= es->patience)
            es->stop = 1;
    }
    return es->stop;
}

/* ============================================================
 * BATCH NORMALIZATION
 * ============================================================ */

typedef struct {
    float *gamma;
    float *beta;
    float *running_mean;
    float *running_var;
    float momentum;
    int size;
} batch_norm_t;

batch_norm_t *batchnorm_create(int size, float momentum) {
    batch_norm_t *bn = kmalloc(sizeof(batch_norm_t));
    if(!bn) return NULL;
    bn->gamma = kmalloc(size * sizeof(float));
    bn->beta = kmalloc(size * sizeof(float));
    bn->running_mean = kmalloc(size * sizeof(float));
    bn->running_var = kmalloc(size * sizeof(float));
    bn->size = size;
    bn->momentum = momentum;
    
    for(int i=0; i<size; i++) {
        bn->gamma[i] = 1.0f;
        bn->beta[i] = 0;
        bn->running_mean[i] = 0;
        bn->running_var[i] = 1.0f;
    }
    return bn;
}

void batchnorm_forward(batch_norm_t *bn, float *input, float *output) {
    if(!bn) return;
    float eps = 0.001f;
    for(int i=0; i<bn->size; i++) {
        float normalized = (input[i] - bn->running_mean[i]) / 
                          (bn->running_var[i] + eps > 0 ? bn->running_var[i] + eps : 0.001f);
        /* sqrt yaklaşık */
        output[i] = bn->gamma[i] * normalized + bn->beta[i];
        
        /* Running average güncelle */
        bn->running_mean[i] = bn->momentum * bn->running_mean[i] + (1-bn->momentum) * input[i];
        float diff = input[i] - bn->running_mean[i];
        bn->running_var[i] = bn->momentum * bn->running_var[i] + (1-bn->momentum) * diff * diff;
    }
}
