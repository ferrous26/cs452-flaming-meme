# -*- coding: utf-8 -*-

task :default => :build

def check_code_name
  return if File.exist?('CODE_NAME')
  File.open('CODE_NAME', 'w') do |fd|
    fd.puts `whoami`.chomp
  end
end

def code_name
  check_code_name
  File.read('CODE_NAME').chomp
end

def increment_version
  version = if File.exist?('VERSION')
              File.read('VERSION').to_i + 1
            else
              0
            end
  File.open('VERSION', 'w') do |fd|
    fd.puts version
  end
end

def rsync_command dest
  "rsync -trulip --exclude '.git/' --exclude './measurement' ./ " +
    (dest == :pi ? 'pi:trains' : 'uw:trains')
end

def path_to dest
  case dest
  when :cowan # the only platform where we need to futz with the path
    paths = ['/u/wbcowan/gnuarm-4.0.2/libexec/gcc/arm-elf/4.0.2',
             '/u/wbcowan/gnuarm-4.0.2/arm-elf/bin']
    "export PATH=#{paths.join(':')}:$PATH"
  end
end

def install_command dest
  case dest
  when :cowan
    ["echo Copying kernel to TFTP server",
     "cp kernel.elf /u/cs452/tftp/ARM/#{UW_USER}/#{ELF_NAME}.elf"]
  else
    []
  end
end

ELF_NAME = code_name + 'OS'
UW_USER  = ENV['UW_USER']

desc "Build #{ELF_NAME}"
task :build, :params do |_, args|
  params = args[:params] || 'psad' # default build options if none specified

  platform = params.include?('π') ? :pi : :cowan
  host     = platform == :pi ? 'pi' : 'uw'

  cmds = []
  env  = []

  # individual flags
  env << 'STRICT=yes'           if params.include? 's'
  env << 'BENCHMARK=yes'        if params.include? 'b'
  env << 'ASSERT=yes'           if params.include? 'a'
  env << 'DEBUG=yes'            if params.include? 'd'
  env << 'RELEASE=1'            if params.include? '1'
  env << 'RELEASE=2'            if params.include? '2'
  env << 'RELEASE=3'            if params.include? '3'
  env << 'RELEASE=4'            if params.include? '4'
  env << 'RELEASE=s'            if params.include? 'x'
  env << 'RELEASE=g'            if params.include? 'g'

  env << (platform == :pi ? 'PI=yes' :  'ARM920=yes')
  env = env.join ' '

  jobs = params.include?('p') ? '-j' : '' # parallelize build

  cmds << path_to(platform)
  cmds << "cd trains"
  cmds << "echo Cleaning Environment"
  cmds << "make clean"
  cmds << "touch Makefile"
  cmds << "echo Building Fresh Kernel"
  cmds << "#{env} make -s #{jobs}"
  cmds.concat install_command platform
  cmds = cmds.compact.join ' && '

  increment_version
  check_code_name
  sh rsync_command(platform)
  sh "ssh #{host} '#{cmds}'"
end

desc 'Debug build with benchmarks'
task(:bench) { Rake::Task[:build].invoke('psadb') }

desc 'Standard release build'
task(:release) { Rake::Task[:build].invoke('p2s') }

desc 'Standard release build with benchmarking enabled'
task(:release_bench) { Rake::Task[:build].invoke('p2sb') }

desc 'Build for the raspberry pi'
task(:pi) { Rake::Task[:build].invoke('πpsad') }


desc 'Generate the PDF guide'
task :guide => 'guide:pdf'

namespace :guide do
  desc 'Generate MD5 hashes of project files'
  task :md5 do
    require 'digest/md5'
    require 'csv'

    md5 = Digest::MD5.new

    [
     ['headers', FileList['include/**/*.h']],
     ['impls',   FileList['src/**/*.c'] + FileList['src/**/*.asm']],
     ['others',  ['README.md', 'Makefile', 'Rakefile'] + FileList['ld/*.ld']]
    ].each do |set, list|
      CSV.open("guide/md5_info_#{set}.csv", 'w') do |csv|
        csv << ['file', 'hash']
          list.sort.each do |file|
          csv << [file.gsub(/_/, '\_'), md5.hexdigest(File.read file)]
        end
      end

      puts "md5sum -> guide/md5_info_#{set}.csv"
    end
  end

  task :_dot do
    sh 'dot -Tpng guide/tasks.dot > guide/tasks.png'
  end

  desc 'Generate the tasks diagram'
  task :dot => :_dot do
    sh 'open guide/tasks.png'
  end

  desc 'Compile the PDF guide'
  task :pdf => [:md5, :_dot] do
    cd 'guide/' do
      sh 'pdflatex guide.tex'
      sh 'open guide.pdf'
    end
  end
end
