// segmenterbin/agglomerative-cluster-plda-adjacency.cc

// Copyright 2017  Vimal Manohar

// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.


#include "base/kaldi-common.h"
#include "util/common-utils.h"
#include "util/stl-utils.h"
#include "tree/cluster-utils.h"
#include "tree/clusterable-classes.h"
#include "ivector/group-clusterable.h"
#include "segmenter/pair-clusterable.h"
#include "segmenter/adjacency-clusterable.h"
#include "segmenter/segmentation.h"

namespace kaldi {

class SegmentClusterer: public BottomUpClusterer {
 public:
  SegmentClusterer(const std::vector<Clusterable*> &points,
                   BaseFloat max_merge_thresh,
                   int32 min_clust,
                   std::vector<Clusterable*> *clusters_out,
                   std::vector<int32> *assignments_out) 
    : BottomUpClusterer(points, max_merge_thresh, min_clust, clusters_out, 
                        assignments_out) { }
 protected:
  virtual bool IsConsideredForMerging(int32 i, int32 j, BaseFloat dist) {
    return ((*clusters_)[i]->MergeThreshold(*((*clusters_)[j]))
            <= max_merge_thresh_);
  }
};

BaseFloat SegmentClusterBottomUp(const std::vector<Clusterable*> &points,
                                 BaseFloat max_merge_thresh,
                                 int32 min_clust,
                                 std::vector<Clusterable*> *clusters_out,
                                 std::vector<int32> *assignments_out) {
  KALDI_ASSERT(min_clust >= 0);
  KALDI_ASSERT(!ContainsNullPointers(points));
  int32 npoints = points.size();
  // make sure fits in uint_smaller and does not hit the -1 which is reserved.
  KALDI_ASSERT(sizeof(uint_smaller)==sizeof(uint32) ||
               npoints < static_cast<int32>(static_cast<uint_smaller>(-1)));

  KALDI_VLOG(2) << "Initializing clustering object.";
  SegmentClusterer bc(points, max_merge_thresh, min_clust, clusters_out, assignments_out);
  BaseFloat ans = bc.Cluster();
  if (clusters_out) KALDI_ASSERT(!ContainsNullPointers(*clusters_out));
  return ans;
}

class CompartmentalizedSegmentClusterer: public CompartmentalizedBottomUpClusterer {
 public:
  CompartmentalizedSegmentClusterer(
      const vector< vector<Clusterable*> > &points, BaseFloat max_merge_thresh,
      int32 min_clust):
    CompartmentalizedBottomUpClusterer(points, max_merge_thresh, min_clust) { }
 protected:
  virtual bool IsConsideredForMerging(int32 comp, int32 i, int32 j, 
                                      BaseFloat dist) {
    return (clusters_[comp][i]->MergeThreshold(*(clusters_[comp][j])) 
            <= max_merge_thresh_);
  }
};

BaseFloat SegmentClusterBottomUpCompartmentalized(
    const std::vector< std::vector<Clusterable*> > &points, BaseFloat thresh,
    int32 min_clust, std::vector< std::vector<Clusterable*> > *clusters_out,
    std::vector< std::vector<int32> > *assignments_out) {
  KALDI_ASSERT(min_clust >= points.size());  // Code does not merge compartments.
  int32 npoints = 0;
  for (vector< vector<Clusterable*> >::const_iterator itr = points.begin(),
           end = points.end(); itr != end; ++itr) {
    KALDI_ASSERT(!ContainsNullPointers(*itr));
    npoints += itr->size();
  }
  // make sure fits in uint_smaller and does not hit the -1 which is reserved.
  KALDI_ASSERT(sizeof(uint_smaller)==sizeof(uint32) ||
               npoints < static_cast<int32>(static_cast<uint_smaller>(-1)));

  CompartmentalizedSegmentClusterer bc(points, thresh, min_clust);
  BaseFloat ans = bc.Cluster(clusters_out, assignments_out);
  if (clusters_out) {
    for (vector< vector<Clusterable*> >::iterator itr = clusters_out->begin(),
             end = clusters_out->end(); itr != end; ++itr) {
      KALDI_ASSERT(!ContainsNullPointers(*itr));
    }
  }
  return ans;
}

void FlattenCompartments(
    const std::vector<std::vector<Clusterable *> > 
      &compartmentalized_clusters,
    std::vector<Clusterable *> *clusterables, 
    std::vector<std::vector<int32> > *compartmentalized_assignment2id) {
  
  int32 num_compartments = compartmentalized_clusters.size();
  compartmentalized_assignment2id->resize(num_compartments);
  for (int32 c = 0, id = 0; c < num_compartments; c++) {
    (*compartmentalized_assignment2id)[c].resize(
        compartmentalized_clusters[c].size());
    for (int32 i = 0; i < compartmentalized_clusters[c].size(); 
         i++, id++) {
      clusterables->push_back(
          compartmentalized_clusters[c][i]);
      (*compartmentalized_assignment2id)[c][i] = id;
    }
  }
}

void SplitClusterToPoints(
    Clusterable *clusterable,
    std::vector<Clusterable *> *split_clusterables) {
  KALDI_ASSERT(clusterable->Type() == "pair");
  KALDI_ASSERT(split_clusterables);

  PairClusterable *pair_clusterable = NULL;
  pair_clusterable = static_cast<PairClusterable*>(clusterable);

  KALDI_ASSERT(pair_clusterable->clusterable1()->Type() == "group");
  KALDI_ASSERT(pair_clusterable->clusterable2()->Type() == "adj");

  GroupClusterable *gc = NULL;
  gc = static_cast<GroupClusterable *>(pair_clusterable->clusterable1());

  AdjacencyClusterable *adjacency_clusterable = NULL;
  adjacency_clusterable = static_cast<AdjacencyClusterable*>(
      pair_clusterable->clusterable2());
  const std::set<int32> &points = adjacency_clusterable->points();
            
  for (std::set<int32>::const_iterator it = points.begin();
       it != points.end(); ++it) {
    std::set<int32> pts;
    pts.insert(*it);
    split_clusterables->push_back(new PairClusterable(
          new GroupClusterable(pts, gc->scores()),
          new AdjacencyClusterable(pts, adjacency_clusterable->start_times(), 
                                   adjacency_clusterable->end_times()),
          pair_clusterable->Weight1(), pair_clusterable->Weight2()));
  }
}

void ClusterOneIter(
    const std::vector<std::vector<Clusterable *> > &clusterables,
    int32 num_clusters_stage1, int32 num_clusters_final, 
    BaseFloat max_merge_threshold,
    std::vector<std::vector<int32> > *assignments_out,
    std::vector<std::vector<Clusterable *> > *clusterables_out = NULL) {
  int32 num_compartments = clusterables.size();

  std::vector<std::vector<Clusterable *> > clusters_stage1;
  std::vector<std::vector<int32> > assignments_stage1;

  SegmentClusterBottomUpCompartmentalized(
      clusterables, max_merge_threshold, 
      std::max(num_compartments, num_clusters_stage1),
      &clusters_stage1, &assignments_stage1);

  std::vector<Clusterable *> clusterables_stage1;
  std::vector<std::vector<int32> > stage1_assignment2id;
  FlattenCompartments(clusters_stage1, &clusterables_stage1, 
                      &stage1_assignment2id);

  std::vector<Clusterable *> clusters_stage2;
  std::vector<int32> assignments_stage2;
  SegmentClusterBottomUp(clusterables_stage1, max_merge_threshold,
                  num_clusters_final,
                  (clusterables_out ? &clusters_stage2 : NULL),
                  &assignments_stage2);

  assignments_out->resize(num_compartments);
  for (int32 c = 0; c < num_compartments; c++) {
    (*assignments_out)[c].resize(assignments_stage1[c].size());
    for (int32 i = 0; i < assignments_stage1[c].size(); i++) {
      int32 stage1_assignment = assignments_stage1[c][i];
      int32 tmp_id = 
        stage1_assignment2id[c][stage1_assignment];
      (*assignments_out)[c][i] = assignments_stage2[tmp_id];
    }
  }
  
  if (clusterables_out) {
    clusterables_out->resize(clusters_stage2.size());
    for (int32 c = 0; c < clusters_stage2.size(); c++) {
      SplitClusterToPoints(clusters_stage2[c], &((*clusterables_out)[c]));
    }
  }
}

}  // end namespace kaldi

int main(int argc, char *argv[]) {
  using namespace kaldi;
  typedef kaldi::int32 int32;
  typedef kaldi::int64 int64;
  try {
    const char *usage =
      "Cluster ivectors using PLDA pairwise distances based on a greedy agglomerative "
      "clustering with search that accounts for the adjacency of segments.\n"
      "TODO better documentation\n"
      "Usage: agglomerative-group-cluster-adjacency [options] "
      "<scores-rspecifier> <reco2utt-rspecifier> "
      "<segmentation-rspecifier> <labels-wspecifier>\n"
      "e.g.: \n"
      " agglomerative-group-cluster-adjacency scp:scores.scp ark:reco2utt \\\n"
      "   \"ark:segmentation-init-from-segments --shift-to-zero=false --frame-overlap=0.0 segments ark:- |\" \\"
      "   ark,t:labels.txt\n";

    ParseOptions po(usage);
    std::string reco2num_spk_rspecifier, utt2num_frames_rspecifier;
    BaseFloat threshold = 0;
    int32 compartment_size = 0;
    int32 num_iters = 3;
    int32 num_clusters_intermediate = 256;
    BaseFloat adjacency_factor = 0.01;

    po.Register("compartment-size", &compartment_size, 
                "If specified, first cluster within compartments of this size.");
    po.Register("reco2num-spk-rspecifier", &reco2num_spk_rspecifier,
      "If supplied, clustering creates exactly this many clusters for each"
      "utterance and the option --threshold is ignored.");
    po.Register("utt2num-frames-rspecifier", &utt2num_frames_rspecifier,
                "The number of frames in each utterance.");
    po.Register("threshold", &threshold, 
                "Merging clusters if their distance"
                "is less than this threshold.");
    po.Register("num-iters", &num_iters, "Number of iterations of clustering");
    po.Register("num-clusters-intermediate", &num_clusters_intermediate,
                "Cluster first into this many clusters using "
                "compartmentalized bottom-up clustering.");
    po.Register("adjacency-factor", &adjacency_factor, 
                "Scale of adjacency penalty in the objective function.");

    po.Read(argc, argv);

    if (po.NumArgs() != 4) {
      po.PrintUsage();
      exit(1);
    }

    std::string scores_rspecifier = po.GetArg(1),
      reco2utt_rspecifier = po.GetArg(2),
      segmentation_rspecifier = po.GetArg(3),
      label_wspecifier = po.GetArg(4);
    
    SequentialBaseFloatMatrixReader scores_reader(scores_rspecifier);
    RandomAccessTokenVectorReader reco2utt_reader(reco2utt_rspecifier);
    segmenter::RandomAccessSegmentationReader segmentation_reader(
        segmentation_rspecifier);
    RandomAccessInt32Reader reco2num_spk_reader(reco2num_spk_rspecifier);
    RandomAccessInt32Reader utt2num_frames_reader(utt2num_frames_rspecifier);
    Int32Writer label_writer(label_wspecifier);

    int32 num_err = 0, num_done = 0;
    for (; !scores_reader.Done(); scores_reader.Next()) {
      const std::string &reco = scores_reader.Key();
      
      if (!reco2utt_reader.HasKey(reco)) {
        KALDI_WARN << "Could not find recording " << reco << " in "
                   << "reco2utt-rspecifier " << reco2utt_rspecifier;
        num_err++;
        continue;
      }
      
      Matrix<BaseFloat> scores(scores_reader.Value());

      // Convert scores into distances.
      scores.Scale(-1.0);
      scores.Sigmoid(scores);

      const std::vector<std::string> &uttlist = reco2utt_reader.Value(reco);
      
      Vector<BaseFloat> start_times(uttlist.size());
      Vector<BaseFloat> end_times(uttlist.size());

      for (size_t i = 0; i < uttlist.size(); i++) {
        const std::string &utt = uttlist[i];

        if (!segmentation_reader.HasKey(utt)) {
          KALDI_ERR << "Could not find start and end frames for "
                    << "utterance " << utt
                    << " in archive " << segmentation_rspecifier << "; "
                    << "skipping utterance.";
        }

        const segmenter::Segmentation &seg = segmentation_reader.Value(utt);
        
        if (seg.Dim() != 1) {
          KALDI_ERR << "segmentation is not kaldi segments converted to "
                    << "Segmentation format.";
        }
        
        start_times(i) = seg.Begin()->start_frame;
        end_times(i) = seg.Begin()->end_frame;
      }

      int32 this_num_utts = uttlist.size();
      std::vector<int32> utt2compartment(this_num_utts);
        
      std::vector<std::vector<Clusterable*> > clusterables;
      std::vector<Clusterable*> clusterables_simple;

      if (compartment_size > 0) {
        int32 num_compartments = 
          (this_num_utts + compartment_size - 1) / compartment_size;
        clusterables.resize(num_compartments);
      }

      for (size_t i = 0; i < this_num_utts; i++) {
        utt2compartment[i] = compartment_size > 0 ? i / compartment_size : i;

        std::set<int32> points;
        points.insert(i);
        if (compartment_size > 0) {
          int32 compartment = i / compartment_size;
          clusterables[compartment].push_back(new PairClusterable(
                new GroupClusterable(points, &scores),
                new AdjacencyClusterable(points, &start_times, &end_times),
                1.0, -adjacency_factor));
        } else {
          clusterables_simple.push_back(new PairClusterable(
              new GroupClusterable(points, &scores),
              new AdjacencyClusterable(points, &start_times, &end_times),
              1.0, -adjacency_factor));
        }
      }

      int32 this_num_speakers = 1;
      if (!reco2num_spk_rspecifier.empty()) {
        this_num_speakers = reco2num_spk_reader.Value(reco);
      } 

      if (compartment_size > 0) {
        for (int32 iter = 0; iter < num_iters; iter++) {
          std::vector<std::vector<Clusterable *> > clusterables_out;
          std::vector<std::vector<int32> > assignments_out;

          int32 num_compartments = clusterables.size();
          ClusterOneIter(
              clusterables, num_clusters_intermediate,
              iter < num_iters - 1 ? num_compartments : this_num_speakers,
              (!reco2num_spk_rspecifier.empty()) 
              ? std::numeric_limits<BaseFloat>::max() : 1.0 / (1 + Exp(threshold)), 
              &assignments_out,
              iter < num_iters ? &clusterables_out : NULL);

          for (int32 c = 0; c < num_compartments; c++ ) {
            for (int32 i = 0; i < assignments_out[c].size(); i++) {
              PairClusterable *pair_clusterable  = NULL;
              pair_clusterable = static_cast<PairClusterable*>(
                  clusterables[c][i]);

              KALDI_ASSERT(pair_clusterable->clusterable2()->Type() == "adj");
              
              AdjacencyClusterable *adjacency_clusterable =
                static_cast<AdjacencyClusterable*> (pair_clusterable->clusterable2());
              const std::set<int32> &points = adjacency_clusterable->points();
              
              KALDI_ASSERT(points.size() == 1);
              utt2compartment[*(points.begin())] = 
                assignments_out[c][i];
            }
          }

          for (int32 c = 0; c < num_compartments; c++) {
            DeletePointers(&(clusterables[c]));
          }
          
          if (iter < num_iters - 1) {
            clusterables.clear();
            clusterables = clusterables_out;
          }
        }
      } else {
        SegmentClusterBottomUp(clusterables_simple, 
                        (!reco2num_spk_rspecifier.empty()) 
                        ? std::numeric_limits<BaseFloat>::max() : 1.0 / (1 + Exp(threshold)), 
                        this_num_speakers,
                        NULL, &utt2compartment);
      }

      for (size_t i = 0; i < this_num_utts; i++) {
        label_writer.Write(uttlist[i], utt2compartment[i]);
      }

      num_done++;
    }

    return (num_done > 0 ? 0 : 1);
  } catch(const std::exception &e) {
    std::cerr << e.what();
    return -1;
  }
}
