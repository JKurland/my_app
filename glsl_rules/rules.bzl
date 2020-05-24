def _impl(ctx):
    user_args = ctx.actions.args()
    user_args.add_all(ctx.attr.compiler_args)

    output_files = []
    for src_target in ctx.attr.srcs:
        src = src_target.files.to_list()[0]
        output_file = ctx.actions.declare_file("{}.spv".format(src.basename))
        args = ctx.actions.args()
        args.add_all([src.path, "-o", output_file.path, "-V"])

        compiler = ctx.attr.spirv_compiler.files.to_list()[0]
        ctx.actions.run(
            outputs = [output_file],
            inputs = [src],
            tools = [compiler],
            executable = compiler.path,
            arguments = [args, user_args],
        )
        output_files.append(output_file)
    return [DefaultInfo(runfiles = ctx.runfiles(output_files))]

spirv = rule(
    implementation = _impl,
    attrs = {
        "spirv_compiler": attr.label(mandatory = True),
        "srcs": attr.label_list(allow_files = True),
        "compiler_args": attr.string_list(default = []),
    },
)
