use bullet::{
    game::{inputs::Chess768, outputs::MaterialCount},
    nn::optimiser::AdamW,
    trainer::{
        save::SavedFormat,
        schedule::{TrainingSchedule, TrainingSteps, lr, wdl},
        settings::LocalSettings,
    },
    value::ValueTrainerBuilder,
    value::loader::sfbinpack::{MoveType, PieceType, SfBinpackLoader, TrainingDataEntry},
};

fn main() {
    let hl_size = 1024;
    let initial_lr = 0.0007;
    let final_lr = 0.00015;
    let start_superbatch = 401;
    let superbatches = 800;
    let wdl_proportion = 0.75;
    const NUM_OUTPUT_BUCKETS: usize = 8; // output bucket training time bad :(

    let mut trainer = ValueTrainerBuilder::default()
        .dual_perspective()
        .optimiser(AdamW)
        .inputs(Chess768)
        .output_buckets(MaterialCount::<NUM_OUTPUT_BUCKETS>)
        .save_format(&[
            SavedFormat::id("l0w").round().quantise::<i16>(255),
            SavedFormat::id("l0b").round().quantise::<i16>(255),
            // we want to save output-bucketed weights in a format
            // that is suitable for fast cpu inference
            SavedFormat::id("l1w")
                .round()
                .quantise::<i16>(64)
                .transpose(),
            SavedFormat::id("l1b").round().quantise::<i16>(255 * 64),
        ])
        .loss_fn(|output, target| output.sigmoid().squared_error(target))
        .build(|builder, stm_inputs, ntm_inputs, output_buckets| {
            // weights
            let l0 = builder.new_affine("l0", 768, hl_size);
            let l1 = builder.new_affine("l1", 2 * hl_size, NUM_OUTPUT_BUCKETS);

            // inference
            let stm_hidden = l0.forward(stm_inputs).screlu();
            let ntm_hidden = l0.forward(ntm_inputs).screlu();
            let hidden_layer = stm_hidden.concat(ntm_hidden);
            l1.forward(hidden_layer).select(output_buckets)
        });

    let schedule = TrainingSchedule {
        net_id: "output_buckets_1024_8".to_string(),
        eval_scale: 400.0,
        steps: TrainingSteps {
            batch_size: 16384,
            batches_per_superbatch: 8192,
            start_superbatch: start_superbatch,
            end_superbatch: superbatches,
        },
        wdl_scheduler: wdl::ConstantWDL {
            value: wdl_proportion,
        },
        lr_scheduler: lr::CosineDecayLR {
            initial_lr,
            final_lr,
            final_superbatch: superbatches,
        },
        save_rate: 10,
    };

    let settings = LocalSettings {
        threads: 2,
        test_set: None,
        output_directory: "checkpoints",
        batch_queue_size: 256,
    };

    let dataloader = {
        let file_paths = [
            "E:/data/test80-2024-05-may-2tb7p.min-v2.v6.binpack",
            "E:/data/test80-2024-06-jun-2tb7p.min-v2.v6.binpack",
        ];
        let buffer_size_mb = 1024;
        let threads = 4;
        fn filter(entry: &TrainingDataEntry) -> bool {
            entry.ply >= 16
                && !entry.pos.is_checked(entry.pos.side_to_move())
                && entry.score.unsigned_abs() <= 10000
                && entry.mv.mtype() == MoveType::Normal
                && entry.pos.piece_at(entry.mv.to()).piece_type() == PieceType::None
        }

        SfBinpackLoader::new_concat_multiple(&file_paths, buffer_size_mb, threads, filter)
    };

    trainer.load_from_checkpoint("checkpoints/output_buckets_1024_8-400");
    trainer.run(&schedule, &settings, &dataloader);
}
